#include "runner.hh"
#include "task.hh"
#include "descriptors.hh"
#include "channel.hh"
#include <boost/bind.hpp>
#include <iostream>
#include <map>

using namespace fw;

static void connecter(address &addr, channel<int> ch) {
    task::socket s(AF_INET, SOCK_STREAM);
    if (s.connect(addr, 100) == 0) {
        ch.send(0);
    } else {
        ch.send(errno);
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

static void collecter(channel<int> ch) {
    std::map<int, unsigned int> results;
    for (int i=0; i<1000; ++i) {
        int result = ch.recv();
        results[result] += 1;
    }
    for (std::map<int, unsigned int>::iterator i=results.begin(); i!=results.end(); ++i) {
        if (i->first == 0) {
            std::cout << "Success: " << i->second << "\n";
        } else {
            std::cout << strerror(i->first) << ": " << i->second << "\n";
        }
    }
    std::cout << std::endl;
    exit(0);
}

int main(int argc, char *argv[]) {
    runner::init();
    channel<int> ch(1000);
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
