#include "task.hh"
#include "descriptors.hh"
#include <iostream>

using namespace fw;

extern const size_t default_stacksize=4096;

static void sleeper() {
    tasksleep(100 + random());
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
