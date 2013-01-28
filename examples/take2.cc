#include "ten/descriptors.hh"
#include "ten/buffer.hh"
#include "ten/http/http_message.hh"

#include <boost/algorithm/string/predicate.hpp>
#include "t2/scheduler.hh"

#include <netinet/tcp.h> // TCP_DEFER_ACCEPT

using namespace ten;
using namespace t2;

namespace t2 {
__thread thread_data *tld = nullptr;
scheduler *the_scheduler = nullptr;
kernel *the_kernel = nullptr;
} // t2

class io_scheduler {
public:
    channel<std::tuple<int, std::shared_ptr<tasklet>>> chan;
    epoll_fd efd;
    event_fd evfd;
    signal_fd sigfd;
    std::thread thread;
public:
    io_scheduler(const io_scheduler&) = delete;
    io_scheduler &operator =(const io_scheduler &) = delete;

    io_scheduler() {
        epoll_event ev{EPOLLIN | EPOLLONESHOT};
        ev.data.fd = evfd.fd;
        efd.add(evfd.fd, ev);

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGQUIT);
        sigfd = signal_fd(mask);

        thread = std::thread{[&] {
            loop();
        }};

        install_sigint_handler();
    }

    ~io_scheduler() {
        thread.join();
    }

    void loop();
    void install_sigint_handler();
    void wait_for_event(int wait_fd, uint32_t event);
};

static io_scheduler &io() {
    static io_scheduler sched;
    return sched;
}

void io_scheduler::wait_for_event(int wait_fd, uint32_t event) {
    tld->self->_post = [=] {
        epoll_event ev{};
        ev.events = event | EPOLLONESHOT;
        ev.data.fd = wait_fd;
        VLOG(5) << "sending self to io chan";
        chan.send(std::make_pair(wait_fd, std::move(tld->self)));
        if (efd.add(wait_fd, ev) < 0) {
            efd.modify(wait_fd, ev);
        }
    };
    tasklet::yield();
}

static void connection(socket_fd fd) {
    VLOG(5) << "in connection handler " << fd.fd;

    buffer buf{4096};
    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    for (;;) {
        buf.reserve(4096);
        ssize_t nr;
        while ((nr = fd.recv(buf.back(), buf.available())) > 0) {
            VLOG(5) << fd.fd << " got " << nr << " bytes\n";
            buf.commit(nr);
            size_t nparse = buf.size();
            req.parse(&parser, buf.front(), nparse);
            buf.remove(nparse);
            if (req.complete) {
                VLOG(5) << " got request " << req.data();
                http_response resp{200, http_headers{"Content-Length", "0"}};
#if 0
                char buf[128];
                struct tm tm;
                time_t now = std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now()
                        );
                strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&now, &tm));
                resp.set("Date", buf);
#endif

                auto conn_hdr = resp.get("Connection");
                if (!conn_hdr || conn_hdr->empty()) {
                    // obey clients wishes if we have none of our own
                    conn_hdr = req.get("Connection");
                    if (conn_hdr)
                        resp.set("Connection", *conn_hdr);
                    else if (req.version == http_1_0)
                        resp.set("Connection", "close");
                }

                std::string data = resp.data();
                ssize_t nw = fd.send(data.data(), data.size());
                (void)nw;

                conn_hdr = resp.get("Connection");
                if (conn_hdr && boost::iequals(*conn_hdr, "close")) {
                    return;
                }
            }
        }
        if (nr == 0) return;
        io().wait_for_event(fd.fd, EPOLLIN);
    }
}

static void accepter(socket_fd fd) {
    DVLOG(5) << "in accepter";
    fd.setnonblock(); // needed because of dup()
    for (;;) {
        io().wait_for_event(fd.fd, EPOLLIN);
        address addr;
        int nfd = fd.accept(addr, SOCK_NONBLOCK);
        // accept can still fail even though EPOLLIN triggered
        if (nfd != -1) {
            DVLOG(5) << "accepted: " << nfd;
            tasklet::spawn_link(connection, nfd);
        }
    }
}

void io_scheduler::loop() {
    std::vector<std::shared_ptr<tasklet>> io_tasks;
    std::vector<epoll_event> events;
    events.reserve(1000);
    for (;;) {
        events.resize(1000);
        efd.wait(events, -1);
        auto new_tasks = chan.recv_all();
        for (auto &tup : new_tasks) {
            int fd = std::get<0>(tup);
            std::shared_ptr<tasklet> t = std::move(std::get<1>(tup));
            if ((size_t)fd >= io_tasks.size()) {
                io_tasks.resize(fd+1);
            }
            DVLOG(5) << "inserting task for io on fd " << fd;
            io_tasks[fd] = std::move(t);
        }

        for (epoll_event &ev : events) {
            int fd = ev.data.fd;
            DVLOG(5) << "events " << ev.events << " on fd " << fd;
            if (fd == evfd.fd) {
                uint64_t e = evfd.read();
                DVLOG(5) << "event " << e << " on eventfd";
                if (e % 7 == 0) return;
                continue;
            }
            CHECK((size_t)fd < io_tasks.size()) << fd << " larger than " << io_tasks.size();
            // TODO: send them all in one go
            if (io_tasks[fd]) {
                if (io_tasks[fd]->_ready.exchange(true) == false) {
                    the_scheduler->add(std::move(io_tasks[fd]));
                }
            }
        }
    }
}

void io_scheduler::install_sigint_handler() {
    tasklet::spawn([&] {
        LOG(INFO) << "waiting for signal";
        wait_for_event(sigfd.fd, EPOLLIN);
        LOG(INFO) << "signal handler";
        the_kernel->shutdown = true;
        evfd.write(7);
        thread.join();
    });
}

int main() {
    kernel k;
    the_kernel = &k;
    scheduler sched;
    the_scheduler = &sched;
    DVLOG(5) << "take2";

    socket_fd listen_fd{AF_INET, SOCK_STREAM};
    address addr{"0.0.0.0", 7700};
    listen_fd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    listen_fd.setsockopt(SOL_SOCKET, TCP_DEFER_ACCEPT, 5);
    listen_fd.bind(addr);
    LOG(INFO) << "bound to " << addr;
    listen_fd.listen();

    for (int i=0; i<4; ++i) {
        tasklet::spawn(accepter, dup(listen_fd.fd));
    }
}

