#include "runner.hh"
#include "task.hh"
#include "error.hh"

using namespace fw;

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
    runner::init();
    // need 16k stack for fprintf of the backtrace
    task::spawn(stack_overflow, 0, 16*1024);
    runner::main();
    return 0;
}