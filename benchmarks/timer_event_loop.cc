#include "ten/task/compat.hh"
#include "ten/descriptors.hh"
#include <iostream>

using namespace ten;
using namespace ten::compat;

static void sleeper() {
    uint64_t ms = random() % 1000;
    tasksleep(ms);
}

static void counter_task(uint64_t &counter) {
    for (;;) {
        taskyield();
        ++counter;
    }
}

static void yield_task() {
    uint64_t counter = 0;
    taskspawn(std::bind(counter_task, std::ref(counter)));
    for (;;) {
        tasksleep(1000);
        std::cout << "counter: " << counter << "\n";
        counter = 0;
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(yield_task);
    for (int i=0; i<1000; ++i) {
        taskspawn(sleeper);
    }
    return p.main();
}
