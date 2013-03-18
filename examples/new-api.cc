#include "ten/task.hh"

int main() {
    using namespace std::chrono;
    using namespace ten;
    task::spawn([] {
            this_task::sleep_for(seconds{1});
    });
}
