#include "ten/task2/coroutine.hh"
#include "ten/task2/task.hh"

#include <memory>
#include <thread>
#include <iostream>

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

#if 0
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
#endif

int main() {
    using namespace ten::task2;
    runtime run;

    runtime::spawn(bar);
    runtime::spawn(foo, 1, 2);
    runtime::spawn(foo, 4, 4);
    run();
}
