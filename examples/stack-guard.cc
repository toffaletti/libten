#include "runner.hh"
#include "task.hh"

static void stack_overflow() {
    char buf[4*1024];
    // this will attempt to write to the guard page
    // and cause a segmentation fault
    char crash = 1;
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(stack_overflow, NULL, 4*1024);
    runner::main();
    return 0;
}