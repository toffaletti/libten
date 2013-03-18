#include "ten/task.hh"
#include "ten/error.hh"
#include "ten/task/main.icc"

using namespace ten;

void go_crazy() {
    throw errorx("weee!");
}

static void unnamed() {
    go_crazy();
}

int taskmain(int argc, char *argv[]) {
    task::spawn(unnamed);
    return EXIT_SUCCESS;
}
