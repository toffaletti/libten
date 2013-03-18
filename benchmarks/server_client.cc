#include "ten/net.hh"
#include "ten/channel.hh"
#include <iostream>
#include <unordered_map>

using namespace ten;

static void connecter(const address &addr, channel<int> ch) {
    try {
        netsock s(AF_INET, SOCK_STREAM);
        if (s.connect(addr, std::chrono::milliseconds{100}) == 0) {
            ch.send(0);
        } else {
            ch.send(std::move(errno));
        }
    } catch (errorx &e) {
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
            task::spawn([=] {
                handler(fd);
            });
        }
    }
}

static void connecter_spawner(const address &addr, const channel<int> &ch) {
    for (int i=0; i<1000; ++i) {
        task::spawn([=] {
            connecter(addr, ch);
        });
    }
}

int main(int argc, char *argv[]) {
    task::main([] {
        channel<int> ch(1000);
        address addr{AF_INET};
        netsock s(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
        s.bind(addr);
        s.listen();
        s.getsockname(addr);
        task listen_task = task::spawn([&] {
            listener(s);
        });
        this_task::yield(); // let listener get setup
        std::thread connecter_thread = task::spawn_thread([=] {
            connecter_spawner(addr, ch);
        });

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
        listen_task.cancel();
        connecter_thread.join();
    });
}
