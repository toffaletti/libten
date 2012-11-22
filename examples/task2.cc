#include "ten/task2/coroutine.hh"

#include <memory>
#include <thread>
#include <sys/syscall.h>
#include <iostream>

bool is_main_thread() noexcept {
    return getpid() == syscall(SYS_gettid);
}

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
    using namespace this_coro;
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

