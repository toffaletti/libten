#include "runner.hh"
#include "task.hh"
#include "descriptors.hh"
#include <boost/bind.hpp>
#include <iostream>

static void sleeper() {
    task::sleep(100 + random());
}

static void counter_task(uint64_t &counter) {
    for (;;) {
        task::yield();
        ++counter;
    }
}

static void yield_task() {
    uint64_t counter = 0;
    task::spawn(boost::bind(counter_task, boost::ref(counter)));
    for (;;) {
        task::sleep(1000);
        std::cout << "counter: " << counter << "\n";
        counter = 0;
    }
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(yield_task);
    for (int i=0; i<1000; ++i) {
        task::spawn(sleeper);
    }
    return runner::main();
}
