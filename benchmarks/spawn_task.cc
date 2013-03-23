#include "ten/task.hh"
#include <iostream>

using namespace ten;

int main() {
    return task::main([] {
        intmax_t count=0;
        try {
            for (;;) {
                task::spawn([] {});
                ++count;
            }
        } catch (std::bad_alloc &) {}
        std::cout << count << "\n";
    });
}
