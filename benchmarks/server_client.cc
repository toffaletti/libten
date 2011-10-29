#include "task.hh"
#include "net.hh"
#include "channel.hh"
#include <iostream>
#include <unordered_map>

using namespace ten;

const size_t default_stacksize=4096;

static void connecter(address &addr, channel<int> ch) {
    netsock s(AF_INET, SOCK_STREAM);
    if (s.connect(addr, 100) == 0) {
        ch.send(0);
    } else {
        ch.send(std::move(errno));
    }
}

static void handler(int fd) {
    netsock s(fd);
    char buf[32];
    for (;;) {
        ssize_t nr = s.recv(buf, sizeof(buf));
        if (nr <= 0) break;
    }
}

static void listener(netsock &sock) {
    address addr;
    for (;;) {
        int fd = sock.accept(addr);
        if (fd != -1) {
            taskspawn(std::bind(handler, fd));
        }
    }
}

static void connecter_spawner(address &addr, channel<int> &ch) {
    for (int i=0; i<1000; ++i) {
        taskspawn(std::bind(connecter, std::ref(addr), ch));
    }
}

static void startup() {
    channel<int> ch(1000);
    address addr;
    netsock s(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
    s.bind(addr);
    s.listen();
    s.getsockname(addr);
    int listen_task = taskspawn(std::bind(listener, std::ref(s)));
    taskyield(); // let listener get setup
    procspawn(std::bind(connecter_spawner, addr, ch));

    std::unordered_map<int, unsigned int> results;
    for (unsigned i=0; i<1000; ++i) {
        int result = ch.recv();
        results[result] += 1;
    }
    for (auto i=results.begin(); i!=results.end(); ++i) {
        if (i->first == 0) {
            std::cout << "Success: " << i->second << "\n";
        } else {
            std::cout << strerror(i->first) << ": " << i->second << "\n";
        }
    }
    std::cout << std::endl;
    taskcancel(listen_task);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(startup);
    return p.main();
}
