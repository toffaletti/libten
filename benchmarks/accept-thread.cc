#include "ten/task.hh"
#include "ten/channel.hh"
#include "ten/net.hh"
#include "ten/logging.hh"
#include <iostream>

using namespace ten;

void client_handler(channel<int> accept_chan) {
    for (;;) {
        int sock = accept_chan.recv();
        close(sock);
        VLOG(2) << "closed " << sock;
    }
}

void listen_task(channel<int> accept_chan) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 3080);
    s.bind(addr);
    s.getsockname(addr);
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        address client_addr;
        int sock;
        while ((sock = s.accept(client_addr, SOCK_NONBLOCK)) > 0) {
            VLOG(1) << "accepted " << sock;
            accept_chan.send(std::move(sock));
        }
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    channel<int> accept_chan(1024);
    taskspawn(std::bind(listen_task, accept_chan));
    procspawn(std::bind(client_handler, accept_chan));
    return p.main(argc, argv);
}

