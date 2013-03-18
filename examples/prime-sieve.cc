#include "ten/channel.hh"
#include <iostream>
#include "ten/task/main.icc"

using namespace ten;

// adapted from http://golang.org/doc/go_tutorial.html#tmp_360

void generate(channel<int> out) {
    for (int i=2; ; ++i) {
        out.send(std::move(i));
    }
}

void filter(channel<int> in, channel<int> out, int prime) {
    for (;;) {
        int i = in.recv();
        if (i % prime != 0) {
            out.send(std::move(i));
        }
    }
}

int taskmain(int argc, char *argv[]) {
    channel<int> ch;
    task::spawn([=] {
        generate(ch);
    });
    for (int i=0; i<100; ++i) {
        int prime = ch.recv();
        std::cout << prime << "\n";
        channel<int> out;
        task::spawn([=] {
            filter(ch, out, prime);
        });
        ch = out;
    }
    kernel::shutdown();
    return EXIT_SUCCESS;
}

