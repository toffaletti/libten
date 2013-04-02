#include "gtest/gtest.h"
#include <thread>
#include "ten/descriptors.hh"
#include "ten/semaphore.hh"
#include "ten/channel.hh"
#include "ten/task.hh"

using namespace ten;
using namespace std::chrono;

static void bar(std::thread::id p) {
    // cant use EXPECT_TRUE in multi-threaded tests :(
    EXPECT_NE(std::this_thread::get_id(), p);
    this_task::yield();
}

static void foo(std::thread::id p) {
    EXPECT_NE(std::this_thread::get_id(), p);
    task::spawn([p]{
        bar(p);
    });
    this_task::yield();
}

TEST(Task, Constructor) {
    task::main([]{
        std::thread::id main_id = std::this_thread::get_id();
        std::thread t = task::spawn_thread([=]{
            foo(main_id);
        });
        t.join();
    });
}

static void co1(int &count) {
    count++;
    this_task::yield();
    count++;
}

TEST(Task, Schedule) {
    int count = 0;
    task::main([&]{
        for (int i=0; i<10; ++i) {
            task::spawn([&]{ co1(count); });
        }
    });
    EXPECT_EQ(20, count);
}

static void pipe_write(pipe_fd &p) {
    EXPECT_EQ(p.write("test", 4), 4);
}

static void pipe_wait(size_t &bytes) {
    pipe_fd p{O_NONBLOCK};
    task::spawn([&] {
        pipe_write(p);
    });
    fdwait(p.r.fd, 'r');
    char buf[64];
    bytes = p.read(buf, sizeof(buf));
}

TEST(Task, Fdwait) {
    size_t bytes = 0;
    task::main([&] {
        task::spawn([&] { pipe_wait(bytes); });
    });
    EXPECT_EQ(bytes, 4);
}

static void connect_to(address addr) {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setnonblock();
    if (s.connect(addr) == 0) {
        // connected!
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        EXPECT_TRUE(fdwait(s.fd, 'w'));
    } else {
        throw errno_error();
    }
}

static void listen_co(bool multithread) {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"127.0.0.1", 0};
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    std::thread connect_thread;
    if (multithread) {
        connect_thread = task::spawn_thread([=] {
            connect_to(addr);
        });
    } else {
        task::spawn([=] {
            connect_to(addr);
        });
    }

    EXPECT_TRUE(fdwait(s.fd, 'r'));

    address client_addr;
    socket_fd cs{s.accept(client_addr, SOCK_NONBLOCK)};
    fdwait(cs.fd, 'r');
    char buf[2];
    ssize_t nr = cs.recv(buf, 2);

    if (connect_thread.joinable()) {
        connect_thread.join();
    }

    // socket should be disconnected because
    // when connect_to task is finished
    // close was called on its socket
    EXPECT_EQ(nr, 0);

}

TEST(Task, Socket) {
    task::main([] {
        task::spawn([]{ listen_co(false); });
    });
}

TEST(Task, SocketThreaded) {
    task::main([] {
        task::spawn([]{ listen_co(true); });
    });
}

static void sleeper() {
    auto start = steady_clock::now();
    this_task::sleep_for(milliseconds{10});
    auto end = steady_clock::now();

    // check that at least 10 milliseconds passed
    EXPECT_NEAR(duration_cast<milliseconds>(end-start).count(), 10.0, 2.0);
}

TEST(Task, Sleep) {
    task::main([] {
        task::spawn([]{ sleeper(); });
    });
}

static void listen_timeout_co() {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"127.0.0.1", 0};
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    bool timeout = !fdwait(s.fd, 'r', std::chrono::milliseconds{5});
    EXPECT_TRUE(timeout);
}

TEST(Task, PollTimeout) {
    task::main([] {
        listen_timeout_co();
    });
}

static void sleep_many(uint64_t &count) {
    ++count;
    this_task::sleep_for(milliseconds{5});
    ++count;
    this_task::sleep_for(milliseconds{10});
    ++count;
}

TEST(Task, ManyTimeouts) {
    uint64_t count(0);
    task::main([&] {
        for (int i=0; i<1000; i++) {
            task::spawn([&] {
                sleep_many(count);
            });
        }
    });
    EXPECT_EQ(count, 3000);
}

static void long_sleeper() {
    this_task::sleep_for(seconds{10});
    EXPECT_TRUE(false);
}

static void cancel_sleep() {
    auto t = task::spawn(long_sleeper);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

TEST(Task, CancelSleep) {
    task::main([] {
        cancel_sleep();
    });
}

static void long_sleeper_yield() {
    try {
        this_task::sleep_for(seconds{10});
    } catch (task_interrupted &) {
        // make sure yield doesn't throw again
        this_task::yield();
        EXPECT_TRUE(true);
        this_task::yield();
        EXPECT_TRUE(true);
        throw;
    }
    EXPECT_TRUE(false);
}

static void cancel_once() {
    auto t = task::spawn(long_sleeper_yield);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

TEST(Task, CancelOnlyOnce) {
    task::main([] {
        cancel_once();
    });
}

static void channel_wait() {
    channel<int> c;
    int a = c.recv();
    (void)a;
    EXPECT_TRUE(false);
}

static void cancel_channel() {
    auto t = task::spawn(channel_wait);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

TEST(Task, CancelChannel) {
    task::main([] {
        cancel_channel();
    });
}

static void io_wait() {
    pipe_fd p;
    fdwait(p.r.fd, 'r');
    EXPECT_TRUE(false);
}

static void cancel_io() {
    auto t = task::spawn(io_wait);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

TEST(Task, CancelIO) {
    task::main([] {
        cancel_io();
    });
}

static void yield_loop(uint64_t &counter) {
    for (;;) {
        this_task::yield();
        ++counter;
    }
}

static void yield_timer() {
    uint64_t counter = 0;
    auto t = task::spawn([&]{ yield_loop(counter); });
    this_task::sleep_for(seconds{1});
    t.cancel();
    // tight yield loop should take less than microsecond
    // this test depends on hardware and will probably fail
    // when run under debugging tools like valgrind/gdb
    EXPECT_GT(counter, 100000);
}

TEST(Task, YieldTimer) {
    task::main([]{
        yield_timer();
    });
}

static void deadline_timer() {
    try {
        deadline dl{milliseconds{100}};
        this_task::sleep_for(milliseconds{200});
        EXPECT_TRUE(false);
    } catch (deadline_reached &e) {
        EXPECT_TRUE(true);
    }
}

static void deadline_not_reached() {
    try {
        deadline dl{milliseconds{100}};
        this_task::sleep_for(milliseconds{50});
        EXPECT_TRUE(true);
    } catch (deadline_reached &e) {
        EXPECT_TRUE(false);
    }
    this_task::sleep_for(milliseconds{100});
    EXPECT_TRUE(true);
}

TEST(Task, DeadlineTimer) {
    task::main([] {
        task::spawn(deadline_timer);
        task::spawn(deadline_not_reached);
    });
}

static void deadline_cleared() {
    auto start = steady_clock::now();
    try {
        deadline dl{milliseconds{10}};
        this_task::sleep_for(seconds{20});
        EXPECT_TRUE(false);
    } catch (deadline_reached &e) {
        EXPECT_TRUE(true);
        // if the deadline or timeout aren't cleared,
        // this yield will sleep or throw again
        this_task::yield();
        auto end = steady_clock::now();
        EXPECT_TRUE(true);
        EXPECT_NEAR((float)duration_cast<milliseconds>(end-start).count(), 10.0, 2.0);
    }
}

TEST(Task, DeadlineCleared) {
    task::main([] {
        task::spawn(deadline_cleared);
    });
}

static void deadline_cleared_not_reached() {
    try {
        if (true) {
            deadline dl{milliseconds{100}};
            this_task::sleep_for(milliseconds{20});
            // deadline should be removed at this point
            EXPECT_TRUE(true);
        }
        this_task::sleep_for(milliseconds{100});
        EXPECT_TRUE(true);
    } catch (deadline_reached &e) {
        EXPECT_TRUE(false);
    }
}

TEST(Task, DeadlineClearedNotReached) {
    task::main([] {
        task::spawn(deadline_cleared_not_reached);
    });
}

static void deadline_yield() {
    auto start = steady_clock::now();
    try {
        deadline dl{milliseconds{5}};
        for (;;) {
            this_task::yield();
        }
        EXPECT_TRUE(false);
    } catch (deadline_reached &e) {
        EXPECT_TRUE(true);
        // if the deadline or timeout aren't cleared,
        // this yield will sleep or throw again
        this_task::yield();
        auto end = steady_clock::now();
        EXPECT_TRUE(true);
        EXPECT_NEAR((float)duration_cast<milliseconds>(end-start).count(), 5.0, 2.0);
    }
}

TEST(Task, DeadlineYield) {
    task::main([]{
        task::spawn(deadline_yield);
    });
}

TEST(Task, DeadlineRecentTime) {
    // test to make sure we're using a recent time
    // to create the deadline. if cached time wasn't
    // updated recently enough this test can fail
    task::main([] {
        pipe_fd p{O_NONBLOCK};
        // must use a thread to trigger this
        // otherwise we might update cached time
        auto pipe_thread = std::thread([&] {
            std::this_thread::sleep_for(milliseconds{20});
            size_t nw = p.write("foo", 3);
            (void)nw;
            std::this_thread::sleep_for(milliseconds{5});
            nw = p.write("foo", 3);
            (void)nw;
        });
        bool got_to_deadline = false;
        try {
            char buf[10];
            fdwait(p.r.fd, 'r'); // this will wait ~20ms
            size_t nr = p.read(buf, sizeof(buf));
            (void)nr;
            deadline dl{milliseconds{10}};
            fdwait(p.r.fd, 'r'); // this will wait ~5ms
        } catch (deadline_reached) {
            got_to_deadline = true;
        }
        pipe_thread.join();
        EXPECT_TRUE(!got_to_deadline);
    });
}

TEST(Task, NewApi) {
    task::main([]{
        int i = 1;
        task t = task::spawn([&] {
            ++i;
        });
        this_task::yield();
        EXPECT_EQ(2, i);

        auto start = kernel::now();
        std::thread th = task::spawn_thread([&] {
            // yield to nothing
            this_task::yield();
            ++i;
            this_task::sleep_for(milliseconds{20});
            std::this_thread::sleep_for(milliseconds{20});
        });
        th.join();
        this_task::yield(); // allow cached time to update
        auto stop = kernel::now();
        EXPECT_EQ(3, i);
        EXPECT_NEAR((float)duration_cast<milliseconds>(stop-start).count(), 40.0, 10.0);
    });
}

TEST(Task, Join) {
    task::main([]{
        task t;
        EXPECT_TRUE(t.joinable() == false);
        int foo = 0;
        t = task::spawn([&]{
            this_task::sleep_for(milliseconds{5});
            foo = 42;
        });
        EXPECT_TRUE(t.joinable());
        EXPECT_TRUE(foo == 0);
        t.join();
        EXPECT_TRUE(t.joinable());
        EXPECT_TRUE(foo == 42);
        t.join();
        EXPECT_TRUE(t.joinable());
    });
}

