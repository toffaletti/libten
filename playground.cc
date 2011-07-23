#include "runner.hh"
#include "descriptors.hh"
#include <boost/bind.hpp>

void echo_task(socket_fd &_s) {
    address client_addr;
    socket_fd s = _s.accept(client_addr, SOCK_NONBLOCK);
    char buf[4096];
    for (;;) {
        task::poll(s.fd, EPOLLIN);
        ssize_t nr = s.recv(buf, sizeof(buf));
        if (nr <= 0) break;
        ssize_t nw = s.send(buf, nr);
    }
}

void listen_task() {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    for (;;) {
        task::poll(s.fd, EPOLLIN);
        task::spawn(boost::bind(echo_task, boost::ref(s)));
    }
}

int main(int argc, char *argv[]) {
    task::spawn(listen_task);
    runner::self()->schedule();
    return 0;
}

