#include "task.hh"
#include "error.hh"

using namespace fw;
const size_t default_stacksize=4096;

void go_crazy() {
    throw errorx("weee!");
}

static void unnamed() {
    go_crazy();
}

void stack_overflow() {
    unnamed();
}

int main(int argc, char *argv[]) {
    procmain p;
    // need 16k stack for fprintf of the backtrace
    taskspawn(stack_overflow, 16*1024);
    return p.main(argc, argv);
}
