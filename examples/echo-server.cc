#include "task.hh"
#include "descriptors.hh"
#include "net.hh"
#include <iostream>

using namespace ten;
const size_t default_stacksize=256*1024;

void echo_task(int sock) {
    netsock s(sock);
    char buf[4096];
    for (;;) {
        ssize_t nr = s.recv(buf, sizeof(buf));
        if (nr <= 0) break;
        ssize_t nw = s.send(buf, nr);
    }
}

void listen_task() {
    netsock s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        address client_addr;
        int sock;
        while ((sock = s.accept(client_addr, 0, 60*1000)) > 0) {
            taskspawn(std::bind(echo_task, sock));
        }
        std::cout << "accept timeout reached\n";
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(listen_task);
    return p.main(argc, argv);
}

