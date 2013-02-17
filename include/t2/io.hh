#ifndef TEN_IO_SCHEDULER_HH
#define TEN_IO_SCHEDULER_HH

#include "t2/channel.hh"
#include "ten/descriptors.hh"

namespace t2 {

class io_scheduler;
extern io_scheduler *the_io_scheduler;

class io_scheduler {
public:
    channel<std::tuple<int, std::shared_ptr<tasklet>>> chan;
    ten::epoll_fd efd;
    ten::event_fd evfd;
    std::thread thread;
public:
    io_scheduler(const io_scheduler&) = delete;
    io_scheduler &operator =(const io_scheduler &) = delete;

    io_scheduler() {
        CHECK(the_io_scheduler == nullptr);
        the_io_scheduler = this;
        epoll_event ev{EPOLLIN | EPOLLONESHOT};
        ev.data.fd = evfd.fd;
        efd.add(evfd.fd, ev);

        install_sigint_handler();

        thread = std::thread{[&] {
            loop();
        }};

    }

    ~io_scheduler() {
        thread.join();
    }

    void loop();
    void install_sigint_handler();
    void wait_for_event(int wait_fd, uint32_t event);
};

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
                if (e % 7 == 0) {
                    // ready all tasks, they will be canceled because shutdown
                    for (auto &task : io_tasks) {
                        if (task) {
                            if (task->_ready.exchange(true) == false) {
                                the_scheduler->add(std::move(task));
                            }
                        }
                    }
                    return;
                }
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
        wait_for_event(the_kernel->sigfd.fd, EPOLLIN);
        LOG(INFO) << "signal handler";
        the_kernel->shutdown = true;
        evfd.write(7);
    });
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

} // t2

#endif
