#include "ten/task.hh"
#include "ten/task/main.icc"

using namespace ten;

static void stack_overflow() {
    char buf[256*1024];
    // this will attempt to write to the guard page
    // and cause a segmentation fault
    char crash = 1;

    // -O3 reorders buf and crash, so it will crash here
    buf[0] = 1;
    printf("%s %c\n", buf, crash);
}

int taskmain(int argc, char *argv[]) {
    task::spawn(stack_overflow);
    return EXIT_SUCCESS;
}
