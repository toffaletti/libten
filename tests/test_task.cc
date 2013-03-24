#define BOOST_TEST_MODULE task test
#include <boost/test/unit_test.hpp>
#include <thread>
#include "ten/descriptors.hh"
#include "ten/semaphore.hh"
#include "ten/channel.hh"
#include "ten/task.hh"

using namespace ten;
using namespace std::chrono;

static void bar(std::thread::id p) {
    // cant use BOOST_CHECK in multi-threaded tests :(
    assert(std::this_thread::get_id() != p);
    this_task::yield();
}

static void foo(std::thread::id p) {
    assert(std::this_thread::get_id() != p);
    task::spawn([p]{
        bar(p);
    });
    this_task::yield();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
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

BOOST_AUTO_TEST_CASE(schedule_test) {
    int count = 0;
    task::main([&]{
        for (int i=0; i<10; ++i) {
            task::spawn([&]{ co1(count); });
        }
    });
    BOOST_CHECK_EQUAL(20, count);
}

static void pipe_write(pipe_fd &p) {
    BOOST_CHECK_EQUAL(p.write("test", 4), 4);
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

BOOST_AUTO_TEST_CASE(fdwait_test) {
    size_t bytes = 0;
    task::main([&] {
        task::spawn([&] { pipe_wait(bytes); });
    });
    BOOST_CHECK_EQUAL(bytes, 4);
}

static void connect_to(address addr) {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setnonblock();
    if (s.connect(addr) == 0) {
        // connected!
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        bool success = fdwait(s.fd, 'w');
        assert(success);
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

    bool success = fdwait(s.fd, 'r');
    assert(success);

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
    BOOST_CHECK_EQUAL(nr, 0);

}

BOOST_AUTO_TEST_CASE(socket_io) {
    task::main([] {
        task::spawn([]{ listen_co(false); });
    });
}

BOOST_AUTO_TEST_CASE(socket_io_mt) {
    task::main([] {
        task::spawn([]{ listen_co(true); });
    });
}

static void sleeper() {
    auto start = steady_clock::now();
    this_task::sleep_for(milliseconds{10});
    auto end = steady_clock::now();

    // check that at least 10 milliseconds passed
    // annoyingly have to compare count here because duration doesn't have an
    // ostream << operator
    BOOST_CHECK_CLOSE(duration_cast<milliseconds>(end-start).count(), 10.0, 20.0);
}

BOOST_AUTO_TEST_CASE(task_sleep) {
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
    BOOST_CHECK(timeout);
}

BOOST_AUTO_TEST_CASE(poll_timeout_io) {
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

BOOST_AUTO_TEST_CASE(many_timeouts) {
    uint64_t count(0);
    task::main([&] {
        for (int i=0; i<1000; i++) {
            task::spawn([&] {
                sleep_many(count);
            });
        }
    });
    BOOST_CHECK_EQUAL(count, 3000);
}

static void long_sleeper() {
    this_task::sleep_for(seconds{10});
    BOOST_CHECK(false);
}

static void cancel_sleep() {
    auto t = task::spawn(long_sleeper);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_sleep) {
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
        BOOST_CHECK(true);
        this_task::yield();
        BOOST_CHECK(true);
        throw;
    }
    BOOST_CHECK(false);
}

static void cancel_once() {
    auto t = task::spawn(long_sleeper_yield);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_only_once) {
    task::main([] {
        cancel_once();
    });
}

static void channel_wait() {
    channel<int> c;
    int a = c.recv();
    (void)a;
    BOOST_CHECK(false);
}

static void cancel_channel() {
    auto t = task::spawn(channel_wait);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_channel) {
    task::main([] {
        cancel_channel();
    });
}

static void io_wait() {
    pipe_fd p;
    fdwait(p.r.fd, 'r');
    BOOST_CHECK(false);
}

static void cancel_io() {
    auto t = task::spawn(io_wait);
    this_task::sleep_for(milliseconds{10});
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_io) {
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
    BOOST_CHECK_MESSAGE(counter > 100000, "counter(" << counter << ") > 100000");
}

BOOST_AUTO_TEST_CASE(task_yield_timer) {
    task::main([]{
        yield_timer();
    });
}

static void deadline_timer() {
    try {
        deadline dl{milliseconds{100}};
        this_task::sleep_for(milliseconds{200});
        BOOST_CHECK(false);
    } catch (deadline_reached &e) {
        BOOST_CHECK(true);
    }
}

static void deadline_not_reached() {
    try {
        deadline dl{milliseconds{100}};
        this_task::sleep_for(milliseconds{50});
        BOOST_CHECK(true);
    } catch (deadline_reached &e) {
        BOOST_CHECK(false);
    }
    this_task::sleep_for(milliseconds{100});
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(task_deadline_timer) {
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
        BOOST_CHECK(false);
    } catch (deadline_reached &e) {
        BOOST_CHECK(true);
        // if the deadline or timeout aren't cleared,
        // this yield will sleep or throw again
        this_task::yield();
        auto end = steady_clock::now();
        BOOST_CHECK(true);
        BOOST_CHECK_CLOSE((float)duration_cast<milliseconds>(end-start).count(), 10.0, 20.0);
    }
}

BOOST_AUTO_TEST_CASE(task_deadline_cleared) {
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
            BOOST_CHECK(true);
        }
        this_task::sleep_for(milliseconds{100});
        BOOST_CHECK(true);
    } catch (deadline_reached &e) {
        BOOST_CHECK(false);
    }
}

BOOST_AUTO_TEST_CASE(task_deadline_cleared_not_reached) {
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
        BOOST_CHECK(false);
    } catch (deadline_reached &e) {
        BOOST_CHECK(true);
        // if the deadline or timeout aren't cleared,
        // this yield will sleep or throw again
        this_task::yield();
        auto end = steady_clock::now();
        BOOST_CHECK(true);
        BOOST_CHECK_CLOSE((float)duration_cast<milliseconds>(end-start).count(), 5.0, 10.0);
    }
}

BOOST_AUTO_TEST_CASE(task_deadline_yield) {
    task::main([]{
        task::spawn(deadline_yield);
    });
}

BOOST_AUTO_TEST_CASE(deadline_recent_time) {
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
        BOOST_CHECK(!got_to_deadline);
    });
}

BOOST_AUTO_TEST_CASE(task_new_api) {
    task::main([]{
        int i = 1;
        task t = task::spawn([&] {
            ++i;
        });
        this_task::yield();
        BOOST_CHECK_EQUAL(2, i);

        auto start = kernel::now();
        std::thread th([&] {
            // yield to nothing
            this_task::yield();
            ++i;
            this_task::sleep_for(milliseconds{20});
            std::this_thread::sleep_for(milliseconds{20});
        });
        th.join();
        this_task::yield(); // allow cached time to update
        auto stop = kernel::now();
        BOOST_CHECK_EQUAL(3, i);
        BOOST_CHECK_CLOSE((float)duration_cast<milliseconds>(stop-start).count(), 40.0, 10.0);
    });
}

BOOST_AUTO_TEST_CASE(task_join) {
    task::main([]{
        task t;
        BOOST_CHECK(t.joinable() == false);
        int foo = 0;
        t = task::spawn([&]{
            this_task::sleep_for(milliseconds{5});
            foo = 42;
        });
        BOOST_CHECK(t.joinable());
        BOOST_CHECK(foo == 0);
        t.join();
        BOOST_CHECK(t.joinable());
        BOOST_CHECK(foo == 42);
        t.join();
        BOOST_CHECK(t.joinable());
    });
}

