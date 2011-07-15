#define BOOST_TEST_MODULE thread test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

#include "thread.hh"
#include "descriptors.hh"
#include "semaphore.hh"

#include <iostream>

BOOST_AUTO_TEST_CASE(mutex_test) {
    mutex m;
    mutex::scoped_lock l(m);
    BOOST_CHECK(l.trylock() == false);
}

static void bar(pid_t p) {
    // cant use BOOST_CHECK in multi-threaded tests :(
    assert(thread::self()->id() != p);
    coroutine::yield();
}

static void foo(pid_t p, semaphore &s) {
    assert(thread::self()->id() != p);
    coroutine::spawn(boost::bind(bar, p));
    s.post();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
    semaphore s;
    pid_t pid = thread::self()->id();
    //std::cout << "main pid: " << pid << "\n";
    BOOST_CHECK(pid);
    thread *t = thread::spawn(boost::bind(foo, pid, boost::ref(s)));
    s.wait();
}

static void co1(int &count) {
    count++;
    coroutine::yield();
    count++;
}

BOOST_AUTO_TEST_CASE(scheduler) {
    thread *t = thread::self();
    int count = 0;
    for (int i=0; i<10; ++i) {
        coroutine *c = coroutine::spawn(boost::bind(co1, boost::ref(count)));
    }
    t->schedule(false);
    BOOST_CHECK_EQUAL(20, count);
}

static void mig_co(semaphore &s) {
    thread *start_thread = thread::self();
    pid_t start_pid = start_thread->id();
    coroutine::migrate();
    pid_t end_pid = thread::self()->id();
    //BOOST_CHECK_NE(start_pid, end_pid);
    coroutine::migrate_to(start_thread);
    //BOOST_CHECK_EQUAL(start_pid, thread::self()->id());
    s.post();
}

BOOST_AUTO_TEST_CASE(thread_migrate) {
    semaphore s;
    BOOST_CHECK(thread::count() >= 0);
    thread *t = thread::spawn(boost::bind(mig_co, boost::ref(s)));
    s.wait();
    BOOST_CHECK(thread::count() > 1);
}

static void listen_co() {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 9900);
    //std::cout << "binding to " << addr << "\n";
    s.bind(addr);
    //std::cout << "binded to " << addr << "\n";
    s.listen();
    thread::poll(s.fd, EPOLLIN|EPOLLONESHOT);

    address client_addr;
    socket_fd cs = s.accept(client_addr, SOCK_NONBLOCK);
    //std::cout << "accepted " << client_addr << "\n";
    //std::cout << "client socket: " << cs << "\n";
}

BOOST_AUTO_TEST_CASE(socket_io) {
    coroutine::spawn(listen_co);
    thread *t = thread::self();
    t->schedule(false);
}