#include "ten/task2/task.hh"
#include "ten/logging.hh"

#include <memory>
#include <thread>
#include <iostream>

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
    //baz->join();
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
    runtime::spawn(bar);
    runtime::spawn(foo, 1, 2);
    runtime::spawn(foo, 4, 4);

    //runtime::dump();
    runtime::wait_for_all();

    auto sleep2 = runtime::spawn([]() {
        LOG(INFO) << "sleep for 2 sec\n";;
        this_task::sleep_for(std::chrono::seconds{2});
        // never reached because canceled
        LOG(INFO) << "done\n";
    });

    std::thread t1([&sleep2]() {
            std::this_thread::sleep_for(std::chrono::seconds{1});
            sleep2->cancel();
    });

    runtime::wait_for_all();
    t1.join();

    runtime::spawn([]() {
        try {
            deadline dl{std::chrono::seconds{1}};
            this_task::sleep_for(std::chrono::seconds{2});
        } catch (deadline_reached &e) {
            LOG(INFO) << "deadline reached";
        }

        try {
            deadline dl{std::chrono::seconds{2}};
            this_task::sleep_for(std::chrono::seconds{1});
            dl.cancel();
            LOG(INFO) << "deadline canceled";
            this_task::sleep_for(std::chrono::seconds{5});
            LOG(INFO) << "after deadline canceled";
        } catch (deadline_reached &e) {
        }
    });

    runtime::wait_for_all();

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
    this_task::sleep_for(std::chrono::seconds{10});
    //runtime::wait_for_all();
    runtime::shutdown();
}

