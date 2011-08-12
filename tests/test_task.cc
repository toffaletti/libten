#define BOOST_TEST_MODULE task test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

#include "runner.hh"
#include "descriptors.hh"
#include "semaphore.hh"
#include "channel.hh"

#include <iostream>

static void bar(thread p) {
    // cant use BOOST_CHECK in multi-threaded tests :(
    assert(runner::self().get_thread() != p);
    task::yield();
}

static void foo(thread p, semaphore &s) {
    assert(runner::self().get_thread() != p);
    task::spawn(boost::bind(bar, p));
    task::yield();
    s.post();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
    runner::init();
    semaphore s;
    thread t = runner::self().get_thread();
    BOOST_CHECK(t.id);
    runner::spawn(boost::bind(foo, t, boost::ref(s)));
    s.wait();
    runner::main();
}

static void co1(int &count) {
    count++;
    task::yield();
    count++;
}

BOOST_AUTO_TEST_CASE(schedule_test) {
    runner::init();
    int count = 0;
    for (int i=0; i<10; ++i) {
        task::spawn(boost::bind(co1, boost::ref(count)));
    }
    runner::main();
    BOOST_CHECK_EQUAL(20, count);
}

static void mig_co(semaphore &s) {
    runner start_runner = runner::self();
    thread start_thread = start_runner.get_thread();
    task::migrate();
    thread end_thread = runner::self().get_thread();
    assert(start_thread != end_thread);
    task::migrate(&start_runner);
    assert(start_thread == runner::self().get_thread());
    s.post();
}

BOOST_AUTO_TEST_CASE(runner_migrate) {
    runner::init();
    semaphore s;
    runner::spawn(boost::bind(mig_co, boost::ref(s)));
    s.wait();
    runner::main();
}

static void connect_to(address addr) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setnonblock();
    if (s.connect(addr) == 0) {
        // connected!
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        assert(task::poll(s.fd, EPOLLOUT));
    } else {
        throw errno_error();
    }
}

static void listen_co(bool multithread, semaphore &sm) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    if (multithread) {
        runner::spawn(boost::bind(connect_to, addr));
    } else {
        task::spawn(boost::bind(connect_to, addr));
    }

    assert(task::poll(s.fd, EPOLLIN));

    address client_addr;
    socket_fd cs(s.accept(client_addr, SOCK_NONBLOCK));
    task::poll(cs.fd, EPOLLIN);
    char buf[2];
    ssize_t nr = cs.recv(buf, 2);

    // socket should be disconnected because
    // when connect_to task is finished
    // close was called on its socket
    BOOST_CHECK_EQUAL(nr, 0);
    sm.post();
}

BOOST_AUTO_TEST_CASE(socket_io) {
    runner::init();
    semaphore s;
    task::spawn(boost::bind(listen_co, false, boost::ref(s)));
    runner::main();
    s.wait();
}

BOOST_AUTO_TEST_CASE(socket_io_mt) {
    runner::init();
    semaphore s;
    task::spawn(boost::bind(listen_co, true, boost::ref(s)));
    runner::main();
    s.wait();
}

static void sleeper(semaphore &s) {
    timespec start;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &start));
    task::sleep(10);
    timespec end;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &end));

    timespec passed = end - start;
    BOOST_CHECK_EQUAL(passed.tv_sec, 0);
    // check that at least 10 milliseconds passed
    BOOST_CHECK_GE(passed.tv_nsec, 10*1000000);
    s.post();
}

BOOST_AUTO_TEST_CASE(task_sleep) {
    runner::init();
    semaphore s;
    task::spawn(boost::bind(sleeper, boost::ref(s)));
    runner::main();
    s.wait();
}

BOOST_AUTO_TEST_CASE(timespec_operations) {
    timespec a = {100, 9};
    timespec b = {99, 10};

    BOOST_CHECK(a > b);
    BOOST_CHECK(b < a);

    timespec r = a - b;
    BOOST_CHECK_EQUAL(r.tv_sec, 0);
    BOOST_CHECK_EQUAL(r.tv_nsec, 999999999);

    b += r;
    BOOST_CHECK_EQUAL(a, b);
}

static void listen_timeout_co(semaphore &sm) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    bool timeout = !task::poll(s.fd, EPOLLIN, 5);
    BOOST_CHECK(timeout);
    sm.post();
}

BOOST_AUTO_TEST_CASE(poll_timeout_io) {
    runner::init();
    semaphore s;
    task::spawn(boost::bind(listen_timeout_co, boost::ref(s)));
    runner::main();
    s.wait();
}

static void sleep_many(atomic_count &count) {
    ++count;
    task::sleep(5);
    ++count;
    task::sleep(10);
    ++count;
}

BOOST_AUTO_TEST_CASE(many_timeouts) {
    runner::init();
    atomic_count count(0);
    for (int i=0; i<1000; i++) {
        task::spawn(boost::bind(sleep_many, boost::ref(count)));
    }
    runner::main();
    BOOST_CHECK_EQUAL(count, 3000);
}

static void long_timeout() {
    task::sleep(200);
    // there is a race here, test might fail.
    // should mostly succeed though
    BOOST_CHECK_EQUAL(runner::count(), runner::ncpu());
}

BOOST_AUTO_TEST_CASE(too_many_runners) {
    runner::init();
    atomic_count count(0);
    // lower timeout to make test run faster
    runner::set_thread_timeout(100);
    for (int i=0; i<runner::ncpu()+5; ++i) {
        runner::spawn(boost::bind(sleep_many, boost::ref(count)), true);
    }
    runner::main();
    BOOST_CHECK_EQUAL(count, (runner::ncpu()+5)*3);
    runner::set_thread_timeout(5*1000);
}

static void dial_google() {
    task::socket s(AF_INET, SOCK_STREAM);
    int status = s.dial("www.google.com", 80, 300);
    BOOST_CHECK_EQUAL(status, 0);
}

BOOST_AUTO_TEST_CASE(task_socket_dial) {
    runner::init();
    task::spawn(dial_google);
    runner::main();
}

static void long_sleeper() {
    task::sleep(10000);
    BOOST_CHECK(false);
}

static void cancel_sleep() {
    task t = task::spawn(long_sleeper);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_sleep) {
    runner::init();
    task::spawn(cancel_sleep);
    runner::main();
}

static void channel_wait() {
    channel<int> c;
    int a = c.recv();
    BOOST_CHECK(false);
}

static void cancel_channel() {
    task t = task::spawn(channel_wait);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_channel) {
    runner::init();
    task::spawn(cancel_channel);
    runner::main();
}

static void io_wait() {
    pipe_fd p;
    task::poll(p.r.fd, EPOLLIN);
    BOOST_CHECK(false);
}

static void cancel_io() {
    task t = task::spawn(io_wait);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_io) {
    runner::init();
    task::spawn(cancel_io);
    runner::main();
}

static void migrate_task() {
    task::migrate();
    task::sleep(1000);
    abort();
}

static void cancel_migrate() {
    task t = task::spawn(migrate_task);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_migrate) {
    runner::init();
    task::spawn(cancel_migrate);
    runner::main();
}

static void yield_loop(uint64_t &counter) {
    for (;;) {
        task::yield();
        ++counter;
    }
}

static void yield_timer() {
    uint64_t counter = 0;
    task t = task::spawn(boost::bind(yield_loop, boost::ref(counter)));
    task::sleep(1000);
    t.cancel();
    // tight yield loop should take less than microsecond
    // this test depends on hardware and will probably fail
    // when run under debugging tools like valgrind/gdb
    BOOST_CHECK_MESSAGE(counter > 100000, "counter(" << counter << ") > 100000");
}

BOOST_AUTO_TEST_CASE(task_yield_timer) {
    runner::init();
    task::spawn(yield_timer);
    runner::main();
}
