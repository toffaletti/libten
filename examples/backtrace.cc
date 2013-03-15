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

int main(int argc, char *argv[]) {
    task::spawn(unnamed);
}
