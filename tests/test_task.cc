#define BOOST_TEST_MODULE task test
#include <boost/test/unit_test.hpp>
#include <thread>
#include "descriptors.hh"
#include "semaphore.hh"
#include "channel.hh"
#include "timespec.hh"
#include "task.hh"

using namespace fw;
// needed a larger stack for socket_io_mt
size_t default_stacksize=4096*2;

static void bar(std::thread::id p) {
    // cant use BOOST_CHECK in multi-threaded tests :(
    assert(std::this_thread::get_id() != p);
    taskyield();
}

static void foo(std::thread::id p, semaphore &s) {
    assert(std::this_thread::get_id() != p);
    taskspawn(std::bind(bar, p));
    taskyield();
    s.post();
}

BOOST_AUTO_TEST_CASE(constructor_test) {
    procmain p;
    semaphore s;
    procspawn(std::bind(foo, std::this_thread::get_id(), boost::ref(s)));
    s.wait();
    p.main();
}

static void co1(int &count) {
    count++;
    taskyield();
    count++;
}

BOOST_AUTO_TEST_CASE(schedule_test) {
    procmain p;
    int count = 0;
    for (int i=0; i<10; ++i) {
        taskspawn(std::bind(co1, boost::ref(count)));
    }
    p.main();
    BOOST_CHECK_EQUAL(20, count);
}

static void pipe_write(pipe_fd &p) {
    BOOST_CHECK_EQUAL(p.write("test", 4), 4);
}

static void pipe_wait(size_t &bytes) {
    pipe_fd p(O_NONBLOCK);
    taskspawn(std::bind(pipe_write, std::ref(p)));
    fdwait(p.r.fd, 'r');
    char buf[64];
    bytes = p.read(buf, sizeof(buf));
}

BOOST_AUTO_TEST_CASE(fdwait_test) {
    procmain p;
    size_t bytes = 0;
    taskspawn(std::bind(pipe_wait, std::ref(bytes)));
    p.main();
    BOOST_CHECK_EQUAL(bytes, 4);
}

static void connect_to(address addr) {
    socket_fd s(AF_INET, SOCK_STREAM);
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

static void listen_co(bool multithread, semaphore &sm) {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    if (multithread) {
        procspawn(std::bind(connect_to, addr));
    } else {
        taskspawn(std::bind(connect_to, addr));
    }

    bool success = fdwait(s.fd, 'r');
    assert(success);

    address client_addr;
    socket_fd cs(s.accept(client_addr, SOCK_NONBLOCK));
    fdwait(cs.fd, 'r');
    char buf[2];
    ssize_t nr = cs.recv(buf, 2);

    // socket should be disconnected because
    // when connect_to task is finished
    // close was called on its socket
    BOOST_CHECK_EQUAL(nr, 0);
    sm.post();
}

BOOST_AUTO_TEST_CASE(socket_io) {
    procmain p;
    semaphore s;
    taskspawn(std::bind(listen_co, false, std::ref(s)));
    p.main();
    s.wait();
}

BOOST_AUTO_TEST_CASE(socket_io_mt) {
    procmain p;
    semaphore s;
    taskspawn(std::bind(listen_co, true, std::ref(s)));
    p.main();
    s.wait();
}

static void sleeper(semaphore &s) {
    timespec start;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &start));
    tasksleep(10);
    timespec end;
    THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &end));

    timespec passed = end - start;
    BOOST_CHECK_EQUAL(passed.tv_sec, 0);
    // check that at least 10 milliseconds passed
    BOOST_CHECK_GE(passed.tv_nsec, 10*1000000);
    s.post();
}

BOOST_AUTO_TEST_CASE(task_sleep) {
    procmain p;
    semaphore s;
    taskspawn(std::bind(sleeper, boost::ref(s)));
    p.main();
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

    bool timeout = !fdwait(s.fd, 'r', 5);
    BOOST_CHECK(timeout);
    sm.post();
}

BOOST_AUTO_TEST_CASE(poll_timeout_io) {
    procmain p;
    semaphore s;
    taskspawn(std::bind(listen_timeout_co, std::ref(s)));
    p.main();
    s.wait();
}

static void sleep_many(uint64_t &count) {
    ++count;
    tasksleep(5);
    ++count;
    tasksleep(10);
    ++count;
}

BOOST_AUTO_TEST_CASE(many_timeouts) {
    procmain p;
    uint64_t count(0);
    for (int i=0; i<1000; i++) {
        taskspawn(std::bind(sleep_many, std::ref(count)));
    }
    p.main();
    BOOST_CHECK_EQUAL(count, 3000);
}

#if 0
static void long_timeout() {
    task::sleep(200);
    // TODO: fix?
    // there is a race here, test might fail.
    // should mostly succeed though
    //BOOST_CHECK_EQUAL(runner::count(), runner::ncpu());
}

BOOST_AUTO_TEST_CASE(too_many_runners) {
    procmain p;
    atomic_count count(0);
    // lower timeout to make test run faster
    runner::set_thread_timeout(100);
    for (unsigned int i=0; i<runner::ncpu()+5; ++i) {
        procspawn(std::bind(sleep_many, boost::ref(count)), true);
    }
    taskspawn(long_timeout);
    p.main();
    BOOST_CHECK_EQUAL(count, (runner::ncpu()+5)*3);
    runner::set_thread_timeout(5*1000);
}

static void long_sleeper() {
    task::sleep(10000);
    BOOST_CHECK(false);
}

static void cancel_sleep() {
    task t = taskspawn(long_sleeper);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_sleep) {
    procmain p;
    taskspawn(cancel_sleep);
    p.main();
}

static void channel_wait() {
    channel<int> c;
    int a = c.recv();
    (void)a;
    BOOST_CHECK(false);
}

static void cancel_channel() {
    task t = taskspawn(channel_wait);
    tasksleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_channel) {
    procmain p;
    taskspawn(cancel_channel);
    p.main();
}

static void io_wait() {
    pipe_fd p;
    task::poll(p.r.fd, EPOLLIN);
    BOOST_CHECK(false);
}

static void cancel_io() {
    task t = taskspawn(io_wait);
    task::sleep(10);
    t.cancel();
}

BOOST_AUTO_TEST_CASE(task_cancel_io) {
    procmain p;
    taskspawn(cancel_io);
    p.main();
}

static void yield_loop(uint64_t &counter) {
    for (;;) {
        task::yield();
        ++counter;
    }
}

static void yield_timer() {
    uint64_t counter = 0;
    task t = taskspawn(std::bind(yield_loop, boost::ref(counter)));
    task::sleep(1000);
    t.cancel();
    // tight yield loop should take less than microsecond
    // this test depends on hardware and will probably fail
    // when run under debugging tools like valgrind/gdb
    BOOST_CHECK_MESSAGE(counter > 100000, "counter(" << counter << ") > 100000");
}

BOOST_AUTO_TEST_CASE(task_yield_timer) {
    procmain p;
    taskspawn(yield_timer);
    p.main();
}

static void deadline_timer() {
    try {
        task::deadline deadline(100);
        task::sleep(200);
        BOOST_CHECK(false);
    } catch (task::deadline_reached &e) {
        BOOST_CHECK(true);
    }
}

static void deadline_not_reached() {
    try {
        task::deadline deadline(100);
        task::sleep(50);
        BOOST_CHECK(true);
    } catch (task::deadline_reached &e) {
        BOOST_CHECK(false);
    }
    task::sleep(100);
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(task_deadline_timer) {
    procmain p;
    taskspawn(deadline_timer);
    taskspawn(deadline_not_reached);
    p.main();
}
#endif

void qlocker(qutex &q, rendez &r, int &x) {
    std::unique_lock<qutex> lk(q);
    ++x;
    r.wakeup();
}

bool is_twenty(int &x) { return x == 20; }

void qutex_task_spawn() {
    qutex q;
    rendez r;
    int x = 0;
    for (int i=0; i<20; ++i) {
        procspawn(std::bind(qlocker, std::ref(q), std::ref(r), std::ref(x)));
        taskyield();
    }
    std::unique_lock<qutex> lk(q);
    r.sleep(lk, std::bind(is_twenty, std::ref(x)));
    BOOST_CHECK_EQUAL(x, 20);
}

BOOST_AUTO_TEST_CASE(qutex_test) {
    procmain p;
    taskspawn(qutex_task_spawn);
    p.main();
}

