#include "ten/buffer.hh"
#include "ten/http/http_message.hh"

#include <boost/algorithm/string/predicate.hpp>
#include "t2/scheduler.hh"
#include "t2/io.hh"

#include <netinet/tcp.h> // TCP_DEFER_ACCEPT

using namespace ten;
using namespace t2;

namespace t2 {
__thread thread_data *tld = nullptr;
kernel *the_kernel = nullptr;
scheduler *the_scheduler = nullptr;
io_scheduler *the_io_scheduler = nullptr;
} // t2

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
        the_io_scheduler->wait_for_event(fd.fd, EPOLLIN);
    }
}

static void accepter(socket_fd fd) {
    DVLOG(5) << "in accepter";
    fd.setnonblock(); // needed because of dup()
    for (;;) {
        the_io_scheduler->wait_for_event(fd.fd, EPOLLIN);
        address addr;
        int nfd = fd.accept(addr, SOCK_NONBLOCK);
        // accept can still fail even though EPOLLIN triggered
        if (nfd != -1) {
            DVLOG(5) << "accepted: " << nfd;
            tasklet::spawn_link(connection, nfd);
        }
    }
}


int main() {
    kernel k;
    scheduler sched;
    io_scheduler io_sched;
    DVLOG(5) << "take2";

    socket_fd listen_fd{AF_INET, SOCK_STREAM};
    address addr{"0.0.0.0", 7700};
    listen_fd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    listen_fd.setsockopt(SOL_SOCKET, TCP_DEFER_ACCEPT, 5);
    listen_fd.bind(addr);
    LOG(INFO) << "bound to " << addr;
    listen_fd.listen();

    for (int i=0; i<2; ++i) {
        tasklet::spawn(accepter, dup(listen_fd.fd));
    }
}

