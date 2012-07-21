#include "ten/task.hh"
#include "ten/descriptors.hh"
#include <iostream>

using namespace ten;
const size_t default_stacksize=256*1024;

// this uses fdwait directly instead of netsock

void echo_task(socket_fd &_s) {
    address client_addr;
    socket_fd s{_s.accept(client_addr, SOCK_NONBLOCK)};
    char buf[4096];
    for (;;) {
        fdwait(s.fd, 'r');
        ssize_t nr = s.recv(buf, sizeof(buf));
        if (nr <= 0) break;
        ssize_t nw = s.send(buf, nr);
    }
}

void listen_task() {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"127.0.0.1", 0};
    s.bind(addr);
    s.getsockname(addr);
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        fdwait(s.fd, 'r');
        taskspawn(std::bind(echo_task, std::ref(s)));
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(listen_task);
    return p.main(argc, argv);
}

