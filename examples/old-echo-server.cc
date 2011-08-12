#include "runner.hh"
#include "task.hh"
#include "descriptors.hh"
#include <boost/bind.hpp>
#include <iostream>

// this uses task::poll directly instead of task::socket

void echo_task(socket_fd &_s) {
    address client_addr;
    socket_fd s(_s.accept(client_addr, SOCK_NONBLOCK));
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
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        task::poll(s.fd, EPOLLIN);
        task::spawn(boost::bind(echo_task, boost::ref(s)));
    }
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(listen_task);
    return runner::main();
}

