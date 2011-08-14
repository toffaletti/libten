#include "runner.hh"
#include "task.hh"
#include "descriptors.hh"
#include "channel.hh"
#include <boost/bind.hpp>
#include <iostream>

static void connecter(address &addr, channel<bool> ch) {
    task::socket s(AF_INET, SOCK_STREAM);
    if (s.connect(addr, 100) == 0) {
        ch.send(true);
    } else {
        ch.send(false);
    }
}

static void handler(int fd) {
    task::socket s(fd);
    char buf[32];
    for (;;) {
        ssize_t nr = s.recv(buf, sizeof(buf));
        if (nr <= 0) break;
    }
}

static void listener(task::socket &sock) {
    address addr;
    for (;;) {
        int fd = sock.accept(addr);
        if (fd != -1) {
            task::spawn(boost::bind(handler, fd));
        }
    }
}

static void collecter(channel<bool> ch) {
    int success = 0;
    int failure = 0;
    for (;;) {
        bool result = ch.recv();
        if (result)
            ++success;
        else
            ++failure;
        if (success + failure == 1000) break;
    }
    std::cout << "success: " << success << " failure: " << failure << std::endl;
    exit(0);
}

int main(int argc, char *argv[]) {
    runner::init();
    channel<bool> ch(1000);
    address addr;
    task::socket s(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
    s.bind(addr);
    s.listen();
    s.getsockname(addr);
    task::spawn(boost::bind(listener, boost::ref(s)));
    for (int i=0; i<1000; ++i) {
        runner::spawn(boost::bind(connecter, boost::ref(addr), ch));
    }
    task::spawn(boost::bind(collecter, ch));
    return runner::main();
}
