#include "ten/task2/coroutine.hh"
#include "ten/task2/task.hh"
#include "ten/logging.hh"

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
    runtime::spawn([&]() {
        for (;;) {
            this_task::sleep_for(std::chrono::seconds{1});
            std::cout << count << "\n";
            count = 0;
        }
    });
    for (;;) {
        ++count;
        this_task::yield();
    }
}

int main() {
    runtime run;

    runtime::spawn(bar);
    runtime::spawn(foo, 1, 2);
    runtime::spawn(foo, 4, 4);
    run();

    auto sleep2 = runtime::spawn([]() {
        LOG(INFO) << "sleep for 2 sec\n";;
        this_task::sleep_for(std::chrono::seconds{2});
        LOG(INFO) << "done\n";
    });

    std::thread t1([&sleep2]() {
            std::this_thread::sleep_for(std::chrono::seconds{1});
            sleep2->cancel();
    });

    run();
    t1.join();

    LOG(INFO) << "run2";
    for (int i=0; i<1000; ++i) {
        runtime::spawn([]() {
            for (;;) {
                int ms = random() % 1000;
                this_task::sleep_for(std::chrono::milliseconds{ms});
            }
        });
    }
    runtime::spawn(counter);
    run();
}
#endif
