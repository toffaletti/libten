#include "ten/thread_local.hh"
#include "ten/task/task.hh"
#include "ten/task/runtime.hh"
#include "ten/task/deadline.hh"
#include "ten/logging.hh"
#include "../src/alarm.hh"

#include <memory>
#include <thread>
#include <iostream>

using namespace ten;

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
    auto baz = task::spawn(zaz, "zazzle me");
    this_task::yield();
}

void counter() {
    uint64_t count = 0;
    task::spawn([&]() {
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

struct Stuff {
    int a;

    Stuff() {
        a = 1234;
    }

    ~Stuff() {
        a = 5678;
    }
};

struct stuff_local {};
struct stuff_local2 {};

thread_local<stuff_local, Stuff> stuff;
thread_local<stuff_local2, Stuff> stuff2;

int main() {
    using namespace std::chrono;
    ten::alarm_clock<int, steady_clock> alarms;
#if 0
    {
        auto d1 = alarms.insert(1, steady_clock::now()+seconds(3), nullptr);
        auto d2 = alarms.insert(1, steady_clock::now()+seconds(3), std::runtime_error("wee"));

        alarms.remove(d1);
        LOG(INFO) << "empty? " << alarms.empty();

        alarms.remove(d1);
        LOG(INFO) << "empty? " << alarms.empty();

        alarms.remove(d2);
        LOG(INFO) << "empty? " << alarms.empty();
    }

    {
        auto d1 = alarms.insert(1, steady_clock::now()+seconds(3), nullptr);
        auto d2 = alarms.insert(1, steady_clock::now()+seconds(3), std::runtime_error("wee"));
        alarms.remove(1);
        LOG(INFO) << "empty? " << alarms.empty();
    }

    {
        auto d1 = alarms.insert(1, steady_clock::now()+seconds(3), nullptr);
        auto d2 = alarms.insert(2, steady_clock::now()+seconds(3), std::runtime_error("wee"));
        alarms.tick(steady_clock::now() + seconds(4), [](int v) {
                    LOG(INFO) << "alarm fired! " << v;
                });
        LOG(INFO) << "empty? " << alarms.empty();
    }
#endif

    {
        {
            ten::alarm_clock<int, steady_clock>::scoped_alarm a1(alarms, 1, steady_clock::now()+seconds(3));
            ten::alarm_clock<int, steady_clock>::scoped_alarm a2(alarms, 2, steady_clock::now()+seconds(3));
            a2.cancel();
            alarms.tick(steady_clock::now() + seconds(4), [](int v, std::exception_ptr e) {
                    LOG(INFO) << "alarm fired! " << v;
                });
        }
        LOG(INFO) << "empty? " << alarms.empty();
    }

    task::spawn(bar);
    task::spawn(foo, 1, 2);
    task::spawn(foo, 4, 4);

    //runtime::dump();
    runtime::wait_for_all();

#if 1
    auto sleep2 = task::spawn([]() {
        LOG(INFO) << "sleep for 2 sec but should be canceled in 1\n";;
        this_task::sleep_for(std::chrono::seconds{2});
        // never reached because canceled
        LOG(INFO) << "done\n";
    });

    LOG(INFO) << "stuff: " << stuff->a;
    LOG(INFO) << "stuff2: " << stuff2->a;

    std::thread t1([](task sleep2) {
            LOG(INFO) << "stuff: " << stuff->a;
            LOG(INFO) << "stuff2: " << stuff2->a;
            std::this_thread::sleep_for(std::chrono::seconds{1});
            sleep2.cancel();
    }, std::move(sleep2));

    runtime::wait_for_all();
    t1.join();

    task::spawn([]() {
        LOG(INFO) << "should reach deadline in 1 second";
        try {
            deadline dl{std::chrono::seconds{1}};
            this_task::sleep_for(std::chrono::seconds{2});
        } catch (deadline_reached &e) {
            LOG(INFO) << "deadline reached";
        }

        LOG(INFO) << "deadline should be canceled in 1 second";

        try {
            deadline dl{std::chrono::seconds{2}};
            this_task::sleep_for(std::chrono::seconds{1});
            dl.cancel();
            LOG(INFO) << "deadline canceled";
            LOG(INFO) << "sleeping for 5 seconds...";
            this_task::sleep_for(std::chrono::seconds{5});
            LOG(INFO) << "after deadline canceled";
        } catch (deadline_reached &e) {
        }
    });

    runtime::wait_for_all();

    LOG(INFO) << "run2";
    for (int i=0; i<1000; ++i) {
        task::spawn([]() {
            for (;;) {
                int ms = random() % 1000;
                this_task::sleep_for(std::chrono::milliseconds{ms});
            }
        });
    }
#endif
    task::spawn(counter);
    task::spawn([] {
        this_task::sleep_for(std::chrono::seconds{10});
        runtime::shutdown();
    });
    runtime::wait_for_all();
}

