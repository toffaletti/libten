#define BOOST_TEST_MODULE runner test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

#include "runner.hh"
#include "descriptors.hh"
#include "semaphore.hh"
#include "channel.hh"

#include <iostream>

BOOST_AUTO_TEST_CASE(task_size) {
    printf("sizeof(context) == %ju\n", sizeof(context));
    printf("sizeof(int) = %ju\n", sizeof(int));
    printf("sizeof(coroutine) = %ju\n", sizeof(coroutine));
    printf("sizeof(timespec) = %ju\n", sizeof(timespec));
    printf("sizeof(task) = %ju\n", sizeof(task));
}

BOOST_AUTO_TEST_CASE(mutex_test) {
    mutex m;
    mutex::scoped_lock l(m);
    BOOST_CHECK(l.trylock() == false);
}

static void bar(thread p) {
    // cant use BOOST_CHECK in multi-runnered tests :(
    assert(runner::self()->get_thread() != p);
    task::yield();
}

static void foo(thread p, semaphore &s) {
    assert(runner::self()->get_thread() != p);
    task::spawn(boost::bind(bar, p));
    s.post();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
    semaphore s;
    thread t = runner::self()->get_thread();
    BOOST_CHECK(t.id);
    runner *r = runner::spawn(boost::bind(foo, t, boost::ref(s)));
    s.wait();
}

static void co1(int &count) {
    count++;
    task::yield();
    count++;
}

BOOST_AUTO_TEST_CASE(schedule_test) {
    runner *t = runner::self();
    int count = 0;
    for (int i=0; i<10; ++i) {
        task::spawn(boost::bind(co1, boost::ref(count)));
    }
    t->schedule();
    BOOST_CHECK_EQUAL(20, count);
}

static void mig_co(semaphore &s) {
    runner *start_runner = runner::self();
    thread start_thread = start_runner->get_thread();
    task::migrate();
    thread end_thread = runner::self()->get_thread();
    assert(start_thread != end_thread);
    task::migrate(start_runner);
    assert(start_thread == runner::self()->get_thread());
    s.post();
}

BOOST_AUTO_TEST_CASE(runner_migrate) {
    semaphore s;
    BOOST_CHECK(runner::count() >= 0);
    runner *t = runner::spawn(boost::bind(mig_co, boost::ref(s)));
    s.wait();
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
    socket_fd cs = s.accept(client_addr, SOCK_NONBLOCK);
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
    semaphore s;
    task::spawn(boost::bind(listen_co, false, boost::ref(s)));
    runner *r = runner::self();
    r->schedule();
    s.wait();
}

BOOST_AUTO_TEST_CASE(socket_io_mt) {
    semaphore s;
    task::spawn(boost::bind(listen_co, true, boost::ref(s)));
    runner *r = runner::self();
    r->schedule();
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
    semaphore s;
    task::spawn(boost::bind(sleeper, boost::ref(s)));
    runner *t = runner::self();
    t->schedule();
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
    semaphore s;
    task::spawn(boost::bind(listen_timeout_co, boost::ref(s)));
    runner *r = runner::self();
    r->schedule();
    s.wait();
}

static void sleep_many(int &count) {
    count++;
    task::sleep(5);
    count++;
    task::sleep(10);
    count++;
}

BOOST_AUTO_TEST_CASE(many_timeouts) {
    int count=0;
    for (int i=0; i<1000; i++) {
        task::spawn(boost::bind(sleep_many, boost::ref(count)));
    }
    runner *r = runner::self();
    r->schedule();
    BOOST_CHECK_EQUAL(count, 3000);
}

static void channel_recv(channel &c) {
    intptr_t d = c.recv<uintptr_t>();
    BOOST_CHECK_EQUAL(d, 1875309);
    char *str = c.recv<char *>();
    BOOST_CHECK_EQUAL(str, "hi");
}

static void channel_send(channel &c) {
    static char str[] = "hi";
    c.send(1875309);
    c.send(str);
}

BOOST_AUTO_TEST_CASE(channel_test) {
    channel c(10);
    task::spawn(boost::bind(channel_recv, boost::ref(c)));
    task::spawn(boost::bind(channel_send, boost::ref(c)));
    runner *r = runner::self();
    r->schedule();
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_test) {
    channel c(1);
    task::spawn(boost::bind(channel_recv, boost::ref(c)));
    task::spawn(boost::bind(channel_send, boost::ref(c)));
    runner *r = runner::self();
    r->schedule();
    BOOST_CHECK(c.empty());
}

static void channel_recv_mt(channel &c, semaphore &s) {
    intptr_t d = c.recv<uintptr_t>();
    assert(d == 1875309);
    char *str = c.recv<char *>();
    assert(strcmp(str, "hi") == 0);
    s.post();
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_mt_test) {
    semaphore s;
    channel c(1);
    runner::spawn(boost::bind(channel_recv_mt, boost::ref(c), boost::ref(s)));
    runner::spawn(boost::bind(channel_send, boost::ref(c)));
    s.wait();
    BOOST_CHECK(c.empty());
}

static void channel_multi_send(channel &c) {
    c.send(1234);
    c.send(1234);
}

static void channel_multi_recv(channel &c) {
    for (int i=0; i<5; ++i) {
        intptr_t d = c.recv<uintptr_t>();
        BOOST_CHECK_EQUAL(d, 1234);
    }
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_multiple_senders_test) {
    channel c(1);
    c.send(1234);
    task::spawn(boost::bind(channel_multi_recv, boost::ref(c)));
    task::spawn(boost::bind(channel_multi_send, boost::ref(c)));
    task::spawn(boost::bind(channel_multi_send, boost::ref(c)));
    runner *r = runner::self();
    r->schedule();
    BOOST_CHECK(c.empty());
}
