#include "ten/task/compat.hh"
#include "ten/channel.hh"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <chrono>

using namespace ten;
using namespace ten::compat;
using namespace std;
using namespace std::chrono;

void one_ring(channel<int> chin, channel<int> chout, int m, int n) {
    auto start = high_resolution_clock::now();
    cout << "sending " << m << " messages in ring of " << n << " tasks\n";
    chout.send(0);
    for (;;) {
        int i = chin.recv();
        if (i < m) {
            ++i;
            chout.send(move(i));
        } else {
            chout.close();
            break;
        }
    }
    auto stop = high_resolution_clock::now();
    cout << (n*m) << " messages in " << duration_cast<milliseconds>(stop - start).count() << "ms\n";
}

void ring(channel<int> chin, channel<int> chout) {
    try {
        for (;;) {
            int n = chin.recv();
            chout.send(move(n));
        }
    } catch (channel_closed_error &e) {
        chout.close();
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    channel<int> chin;
    channel<int> chfirst = chin;
    int n = 10;
    int m = 1000;
    if (argc >= 2) {
        n = boost::lexical_cast<int>(argv[1]);
    }
    if (argc >= 3) {
        m = boost::lexical_cast<int>(argv[2]);
    }
    for (int i=0; i<n; ++i) {
        channel<int> chout;
        taskspawn(bind(ring, chin, chout));
        chin = chout;
    }
    taskspawn(bind(one_ring, chin, chfirst, m, n));
    return p.main(argc, argv);
}

