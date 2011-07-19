#define BOOST_TEST_MODULE runner test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

#include "task.hh"
#include "descriptors.hh"
#include "semaphore.hh"

#include <iostream>

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
    t->schedule(false);
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
    BOOST_CHECK(runner::count() > 1);
}

static void connect_to(address addr) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setnonblock();
    if (s.connect(addr) == 0) {
        // connected!
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        task::poll(s.fd, EPOLLOUT|EPOLLONESHOT);
    } else {
        throw errno_error();
    }
}

static void listen_co() {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    //std::cout << "binding to " << addr << "\n";
    s.bind(addr);
    s.getsockname(addr);
    //std::cout << "binded to " << addr << "\n";
    s.listen();

    //task::spawn(boost::bind(connect_to, addr));
    runner::spawn(boost::bind(connect_to, addr));

    //std::cout << "waiting for connection\n";
    task::poll(s.fd, EPOLLIN|EPOLLONESHOT);

    address client_addr;
    socket_fd cs = s.accept(client_addr, SOCK_NONBLOCK);
    //std::cout << "accepted " << client_addr << "\n";
    //std::cout << "client socket: " << cs << "\n";
    task::poll(cs.fd, EPOLLIN|EPOLLONESHOT);
    char buf[2];
    ssize_t nr = cs.recv(buf, 2);
    //std::cout << "nr: " << nr << "\n";

    // socket should be disconnected because
    // when connect_to task is finished
    // close was called on its socket
    BOOST_CHECK_EQUAL(nr, 0);
}

BOOST_AUTO_TEST_CASE(socket_io) {
    task::spawn(listen_co);
    runner *t = runner::self();
    t->schedule(false);
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
    std::cout << "time passed: " << passed.tv_nsec << "\n";
    s.post();
}

BOOST_AUTO_TEST_CASE(task_sleep) {
    semaphore s;
    task::spawn(boost::bind(sleeper, boost::ref(s)));
    runner *t = runner::self();
    t->schedule(false);
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