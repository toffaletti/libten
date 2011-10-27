#define BOOST_TEST_MODULE task test
#include <boost/test/unit_test.hpp>
#include <thread>
#include "descriptors.hh"
#include "semaphore.hh"
#include "channel.hh"
#include "task.hh"

using namespace fw;
// needed a larger stack for socket_io_mt
const size_t default_stacksize=4096*2;

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
    procspawn(std::bind(foo, std::this_thread::get_id(), std::ref(s)));
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
        taskspawn(std::bind(co1, std::ref(count)));
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
    using namespace std::chrono;
    auto start = monotonic_clock::now();
    tasksleep(10);
    auto end = monotonic_clock::now();

    // check that at least 10 milliseconds passed
    // annoyingly have to compare count here because duration doesn't have an
    // ostream << operator
    BOOST_CHECK_GE(duration_cast<milliseconds>(end-start).count(), milliseconds(10).count());
    s.post();
}

BOOST_AUTO_TEST_CASE(task_sleep) {
    procmain p;
    semaphore s;
    taskspawn(std::bind(sleeper, std::ref(s)));
    p.main();
    s.wait();
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

static void long_sleeper() {
    tasksleep(10000);
    BOOST_CHECK(false);
}

static void cancel_sleep() {
    uint64_t id = taskspawn(long_sleeper);
    tasksleep(10);
    taskcancel(id);
}

BOOST_AUTO_TEST_CASE(task_cancel_sleep) {
    procmain p;
    taskspawn(cancel_sleep);
    p.main();
}

static void long_sleeper_yield() {
    try {
        tasksleep(10000);
    } catch (task_interrupted &) {
        // make sure yield doesn't throw again
        taskyield();
        BOOST_CHECK(true);
        taskyield();
        BOOST_CHECK(true);
        throw;
    }
    BOOST_CHECK(false);
}

static void cancel_once() {
    uint64_t id = taskspawn(long_sleeper_yield);
    tasksleep(10);
    taskcancel(id);
}

BOOST_AUTO_TEST_CASE(task_cancel_only_once) {
    procmain p;
    taskspawn(cancel_once);
    p.main();
}

static void channel_wait() {
    channel<int> c;
    int a = c.recv();
    (void)a;
    BOOST_CHECK(false);
}

static void cancel_channel() {
    uint64_t id = taskspawn(channel_wait);
    tasksleep(10);
    taskcancel(id);
}

BOOST_AUTO_TEST_CASE(task_cancel_channel) {
    procmain p;
    taskspawn(cancel_channel);
    p.main();
}

static void io_wait() {
    pipe_fd p;
    fdwait(p.r.fd, 'r');
    BOOST_CHECK(false);
}

static void cancel_io() {
    uint64_t id = taskspawn(io_wait);
    tasksleep(10);
    taskcancel(id);
}

BOOST_AUTO_TEST_CASE(task_cancel_io) {
    procmain p;
    taskspawn(cancel_io);
    p.main();
}

static void yield_loop(uint64_t &counter) {
    for (;;) {
        taskyield();
        ++counter;
    }
}

static void yield_timer() {
    uint64_t counter = 0;
    uint64_t id = taskspawn(std::bind(yield_loop, std::ref(counter)));
    tasksleep(1000);
    taskcancel(id);
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
        deadline dl(100);
        tasksleep(200);
        BOOST_CHECK(false);
    } catch (deadline_reached &e) {
        BOOST_CHECK(true);
    }
}

static void deadline_not_reached() {
    try {
        deadline dl(100);
        tasksleep(50);
        BOOST_CHECK(true);
    } catch (deadline_reached &e) {
        BOOST_CHECK(false);
    }
    tasksleep(100);
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(task_deadline_timer) {
    procmain p;
    taskspawn(deadline_timer);
    taskspawn(deadline_not_reached);
    p.main();
}

static void deadline_cleared() {
    try {
        deadline dl(10);
        tasksleep(20000);
        BOOST_CHECK(false);
    } catch (deadline_reached &e) {
        BOOST_CHECK(true);
        // if the deadline or timeout aren't cleared,
        // this yield will sleep or throw again
        auto start = monotonic_clock::now();
        taskyield();
        auto end = monotonic_clock::now();
        BOOST_CHECK(true);
        BOOST_CHECK_LE(duration_cast<milliseconds>(end-start).count(), seconds(1).count());
    }
}

BOOST_AUTO_TEST_CASE(task_deadline_cleared) {
    procmain p;
    taskspawn(deadline_cleared);
    p.main();
}

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

