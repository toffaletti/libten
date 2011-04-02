#include "descriptors.hh"
#include <iostream>
#include <assert.h>
#include <utility>

int main(int argc, char *argv[]) {

    //fd_base fd1(0);
    //fd_base fd2(std::move(fd1));
    //assert(fd1.fd == -1);
    //assert(fd2.fd != -1);

    epoll_fd efd;

    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    socket_fd sp1(sv[0]);
    socket_fd sp2(sv[1]);

    char a = 1;
    pipe_fd pipe;
    pipe.write(&a, 1);
    assert(pipe.read(&a, 1) == 1);
    printf("read a: %d\n", a);

    timer_fd t;
    itimerspec ts;
    itimerspec prev;
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec = 1;
    ts.it_value.tv_nsec = 50;
    t.settime(0, ts, prev);
    uint64_t fired;
    assert(t.read(&fired, sizeof(fired)) == 8);
    std::cout << "timer fired " << fired << " times\n";

    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 9900);
    std::cout << "binding to " << addr << "\n";
    s.bind(addr);
    std::cout << "binded to " << addr << "\n";
    s.listen();
    address client_addr;
    s.accept(client_addr);
    std::cout << "accepted " << client_addr << "\n";

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    signal_fd sig(sigset);
    std::cout << "waiting for SIGINT\n";
    signalfd_siginfo siginfo;
    sig.read(siginfo);
    std::cout << "got SIGINT from pid: " << siginfo.ssi_pid << "\n";
    return 0;
}
