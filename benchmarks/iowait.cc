#include "ten/task.hh"
#include "ten/descriptors.hh"

int main() {
    using namespace ten;
    using namespace std::chrono;
    return task::main([] {
        pipe_fd p{O_NONBLOCK};
        for (unsigned i=0; i<10; ++i) {
            fdwait(p.r.fd, 'r', milliseconds{100});
        }
    });
}
