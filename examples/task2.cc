#include "ten/task2/coroutine.hh"
#include "ten/task2/task.hh"

#include <memory>
#include <thread>
#include <iostream>

#if 0
void foo(int a, int b) {
    std::cout << "foo: " << a+b << "\n";
    this_coro::yield();
    std::cout << "waaaaa\n";
    for (int i=0; i<10; i++) {
        this_coro::yield();
        std::cout << "lala: " << i << "\n";
    }
}

void zaz(const char *msg) {
    std::cout << msg << "\n";
}

void bar() {
    coroutine baz{zaz, "zazzle me"};
    baz.join();
    this_coro::yield();
}

int main() {
    coroutine _;

    std::cout << "sizeof coro: " << sizeof(coroutine) << "\n";
    coroutine c{foo, 1, 2};
    c.join();

    coroutine barco{bar};
    barco.join();

    std::thread t1([] {
            coroutine _;
            coroutine barco{bar};
            barco.join();
            });
    t1.join();

    std::cout << "end main\n";
}
#else

using namespace ten::task2;

void foo(int a, int b) {
    std::cout << "foo: " << a+b << "\n";
    this_task::yield();
    std::cout << "waaaaa\n";
    for (int i=0; i<10; i++) {
        this_task::yield();
        std::cout << "lala: " << i << "\n";
    }
}

void zaz(const char *msg) {
    std::cout << msg << "\n";
}

void bar() {
    auto baz = runtime::spawn(zaz, "zazzle me");
    baz->join();
    this_task::yield();
}

void counter() {
    uint64_t count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (;;) {
        ++count;
        this_task::yield();
        auto now = std::chrono::high_resolution_clock::now();
        if (now - start >= std::chrono::seconds(1)) {
            std::cout << count << "\n";
            count = 0;
            start = std::chrono::high_resolution_clock::now();
        }
    }
}

int main() {
    runtime run;

    runtime::spawn(bar);
    runtime::spawn(foo, 1, 2);
    runtime::spawn(foo, 4, 4);
    run();

    std::cout << "\nrun2\n";
    runtime::spawn(counter);
    run();
}
#endif
