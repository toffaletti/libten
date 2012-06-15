#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "ten/tagged_ptr.hh"

struct foo {
    int a;
    int b;
    int c;
};

template <typename T>
class tagged_ptr : public ten::tagged_ptr<T, ten::atomic_count_tagger> {
public:
    tagged_ptr(T *p) : ten::tagged_ptr<T, ten::atomic_count_tagger>(p) {}
};

void func() {
    for (int i=0; i<100; ++i) {
        tagged_ptr<foo> f(new foo());
        f->a = 1;
        f->b = 2;
        f->c = 3;
        printf("%lu %u\n", f._data, f.tag());
        f.free();
    }
}

void print_tid() {
    pid_t tid = syscall(SYS_gettid);
    printf("%u\n", tid);
}

int main() {
    std::vector<std::thread> pool;
    for (int i=0; i<4; ++i) {
        pool.push_back(std::thread(func));
    }

    for (int i=0; i<4; ++i) {
        pool[i].join();
    }
    return 0;
}
