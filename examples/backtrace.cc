#include "runner.hh"
#include "task.hh"
#include "error.hh"

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
    task::spawn(stack_overflow);
    runner::main();
    return 0;
}