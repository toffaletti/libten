#include "ten/task.hh"
#include "ten/error.hh"

using namespace ten;

void go_crazy() {
    throw errorx("weee!");
}

static void unnamed() {
    go_crazy();
}

int main() {
    return task::main([] {
        task::spawn(unnamed);
    });
}
