#include "ten/task.hh"
#include "ten/task/main.icc"

int taskmain(int argc, char *argv[]) {
    using namespace std::chrono;
    using namespace ten;
    task::spawn([] {
            this_task::sleep_for(seconds{1});
    });
    return EXIT_SUCCESS;
}
