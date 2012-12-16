#include "ten/task/compat.hh"
#include "ten/error.hh"

using namespace ten;
using namespace ten::compat;

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
