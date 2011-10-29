#include "task.hh"

using namespace ten;
const size_t default_stacksize=4096;

static void stack_overflow() {
    char buf[4*1024];
    // this will attempt to write to the guard page
    // and cause a segmentation fault
    char crash = 1;

    // -O3 reorders buf and crash, so it will crash here
    buf[0] = 1;
    printf("%s %c\n", buf, crash);
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(stack_overflow, 4*1024);
    return p.main(argc, argv);
}
