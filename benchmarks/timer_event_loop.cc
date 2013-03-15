#include "ten/task.hh"
#include "ten/descriptors.hh"
#include <iostream>

using namespace ten;
using namespace std::chrono;

extern const size_t default_stacksize=256*1024;

static void yield_task() {
    uint64_t counter = 0;
    task::spawn([&] {
        for (;;) {
            this_task::yield();
            ++counter;
        }
    });

    for (;;) {
        this_task::sleep_for(seconds{1});
        std::cout << "counter: " << counter << "\n";
        counter = 0;
    }
}

int main(int argc, char *argv[]) {
    task::spawn(yield_task);
    for (int i=0; i<1000; ++i) {
        task::spawn([] {
            this_task::sleep_for(milliseconds{random() % 1000});
        });
    }
}
