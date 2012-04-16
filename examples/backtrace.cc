#include "ten/task.hh"
#include "ten/error.hh"

using namespace ten;
const size_t default_stacksize=256*1024;

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
    taskspawn(stack_overflow);
    return p.main(argc, argv);
}
