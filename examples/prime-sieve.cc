#include "channel.hh"
#include <boost/bind.hpp>
#include <iostream>

using namespace fw;
size_t default_stacksize = 4096;

// adapted from http://golang.org/doc/go_tutorial.html#tmp_360

void generate(channel<int> out) {
    for (int i=2; ; ++i) {
        out.send(i);
    }
}

void filter(channel<int> in, channel<int> out, int prime) {
    for (;;) {
        int i = in.recv();
        if (i % prime != 0) {
            out.send(i);
        }
    }
}

void primes() {
    channel<int> ch;
    taskspawn(boost::bind(generate, ch));
    for (int i=0; i<100; ++i) {
        int prime = ch.recv();
        std::cout << prime << "\n";
        channel<int> out;
        taskspawn(boost::bind(filter, ch, out, prime));
        ch = out;
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(primes);
    return p.main(argc, argv);
}
