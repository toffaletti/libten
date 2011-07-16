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
    assert(runner::self()->id() != p);
    task::yield();
}

static void foo(thread p, semaphore &s) {
    assert(runner::self()->id() != p);
    task::spawn(boost::bind(bar, p));
    s.post();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
    semaphore s;
    thread pid = runner::self()->id();
    BOOST_CHECK(pid.id);
    runner *t = runner::spawn(boost::bind(foo, pid, boost::ref(s)));
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
        task *c = task::spawn(boost::bind(co1, boost::ref(count)));
    }
    t->schedule(false);
    BOOST_CHECK_EQUAL(20, count);
}

static void mig_co(semaphore &s) {
    runner *start_runner = runner::self();
    thread start_pid = start_runner->id();
    task::migrate();
    thread end_pid = runner::self()->id();
    //BOOST_CHECK_NE(start_pid, end_pid);
    task::migrate_to(start_runner);
    //BOOST_CHECK_EQUAL(start_pid, runner::self()->id());
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
        runner::poll(s.fd, EPOLLOUT|EPOLLONESHOT);
    } else {
        throw errno_error();
    }
}

static void listen_co() {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 9900);
    //std::cout << "binding to " << addr << "\n";
    s.bind(addr);
    //std::cout << "binded to " << addr << "\n";
    s.listen();

    //task::spawn(boost::bind(connect_to, addr));
    runner::spawn(boost::bind(connect_to, addr));

    //std::cout << "waiting for connection\n";
    runner::poll(s.fd, EPOLLIN|EPOLLONESHOT);

    address client_addr;
    socket_fd cs = s.accept(client_addr, SOCK_NONBLOCK);
    //std::cout << "accepted " << client_addr << "\n";
    //std::cout << "client socket: " << cs << "\n";
    runner::poll(cs.fd, EPOLLIN|EPOLLONESHOT);
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