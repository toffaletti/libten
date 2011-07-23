#include "runner.hh"
#include "channel.hh"

#include <boost/bind.hpp>

void generate(channel *out) {
    for (int i=2; ; ++i) {
        out->send(i);
    }
}

void filter(channel *in, channel *out, int prime) {
    for (;;) {
        int i = in->recv<uintptr_t>();
        if (i % prime != 0) {
            out->send(i);
        }
    }
}

void primes() {
    channel *ch = new channel;
    task::spawn(boost::bind(generate, ch));
    for (int i=0; i<100; ++i) {
        int prime = ch->recv<uintptr_t>();
        printf("%i\n", prime);
        channel *out = new channel;
        task::spawn(boost::bind(filter, ch, out, prime));
        ch = out;
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    task::spawn(primes);
    runner::self()->schedule();
}