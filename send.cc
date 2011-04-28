#include "descriptors.hh"
#include <iostream>

int main(int argc, char *argv[]) {
    socket_fd s(AF_INET, SOCK_DGRAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address baddr;
    s.bind(baddr);
    std::cout << "bound to " << baddr << std::endl;
    address saddr("0.0.0.0", 9800);
    const char *buf = "hi there";
    ssize_t nw = s.sendto(buf, sizeof(buf), saddr);
    std::cout << "send " << nw << " bytes to " << saddr << std::endl;
    address raddr;
    char rbuf[512];
    ssize_t nr = s.recvfrom(rbuf, sizeof(rbuf), raddr);
    std::cout << "read " << nr << " bytes from " << raddr << std::endl;
}