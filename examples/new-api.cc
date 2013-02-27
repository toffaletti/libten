#include "ten/task.hh"

// TODO: remove this
const size_t default_stacksize = 8*1024;

int main() {
    using namespace std::chrono;
    using namespace ten;
    task::spawn([] {
            this_task::sleep_for(seconds{1});
    });
}
