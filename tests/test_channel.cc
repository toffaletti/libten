#define BOOST_TEST_MODULE channel test
#include <boost/test/unit_test.hpp>
#include "ten/descriptors.hh"
#include "ten/semaphore.hh"
#include "ten/channel.hh"

using namespace ten;
const size_t default_stacksize=245*1024;

static void channel_recv(channel<intptr_t> c, channel<int> done_chan) {
    intptr_t d = c.recv();
    BOOST_CHECK_EQUAL(d, 8675309);
    char *str = reinterpret_cast<char *>(c.recv());
    BOOST_CHECK_EQUAL(str, "hi");
    done_chan.send(1);
}

static void channel_send(channel<intptr_t> c, channel<int> done_chan) {
    static char str[] = "hi";
    c.send(8675309);
    c.send(reinterpret_cast<intptr_t>(str));
    done_chan.send(1);
}

void channel_test_task() {
    channel<int> done_chan;
    channel<intptr_t> c{10};
    taskspawn(std::bind(channel_recv, c, done_chan));
    taskspawn(std::bind(channel_send, c, done_chan));
    for (int i=0; i<2; ++i) {
        done_chan.recv();
    }
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_test) {
    procmain p;
    taskspawn(channel_test_task);
    p.main();
}

void unbuffered_test_task() {
    channel<int> done_chan;
    channel<intptr_t> c;
    taskspawn(std::bind(channel_recv, c, done_chan));
    taskspawn(std::bind(channel_send, c, done_chan));
    for (int i=0; i<2; ++i) {
        done_chan.recv();
    }
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_test) {
    procmain p;
    taskspawn(unbuffered_test_task);
    p.main();
}

static void channel_recv_mt(channel<intptr_t> c, semaphore &s) {
    intptr_t d = c.recv();
    assert(d == 8675309);
    char *str = reinterpret_cast<char *>(c.recv());
    assert(strcmp(str, "hi") == 0);
    s.post();
}

void mt_test_task() {
    semaphore s;
    channel<intptr_t> c;
    channel<int> done_chan;
    procspawn(std::bind(channel_recv_mt, c, std::ref(s)));
    procspawn(std::bind(channel_send, c, done_chan));
    s.wait();
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_mt_test) {
    procmain p;
    taskspawn(mt_test_task);
    p.main();
}

static void channel_multi_send(channel<intptr_t> c, channel<int> done_chan) {
    c.send(1234);
    c.send(1234);
    done_chan.send(1);
}

static void channel_multi_recv(channel<intptr_t> c, channel<int> done_chan) {
    for (int i=0; i<5; ++i) {
        intptr_t d = c.recv();
        BOOST_CHECK_EQUAL(d, 1234);
    }
    BOOST_CHECK(c.empty());
    done_chan.send(1);
}

void multiple_senders_task() {
    channel<int> done_chan;
    channel<intptr_t> c{4};
    c.send(1234);
    taskspawn(std::bind(channel_multi_recv, c, done_chan));
    taskspawn(std::bind(channel_multi_send, c, done_chan));
    taskspawn(std::bind(channel_multi_send, c, done_chan));
    for (int i=0; i<3; ++i) {
        done_chan.recv();
    }
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_multiple_senders_test) {
    procmain p;
    taskspawn(multiple_senders_task);
    p.main();
}

static void delayed_channel_send(channel<int> c) {
    tasksleep(100);
    c.send(5309);
}

static void delayed_channel(address addr) {
    channel<int> c;
    // spawn the send in another thread before blocking on recv
    procspawn(std::bind(delayed_channel_send, c));
    int a = c.recv();
    (void)a;

    socket_fd s{AF_INET, SOCK_STREAM};
    s.setnonblock();
    if (s.connect(addr) == 0) {
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        bool success = fdwait(s.fd, 'w');
        assert(success);
    } else {
        throw errno_error();
    }
}

static void wait_on_io() {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"127.0.0.1", 0};
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    taskspawn(std::bind(delayed_channel, addr));
    fdwait(s.fd, 'r');
}

BOOST_AUTO_TEST_CASE(blocked_io_and_channel) {
    procmain p;
    taskspawn(wait_on_io);
    p.main();
}

static void channel_closer_task(channel<int> c, int &closed) {
    taskyield();
    c.close();
    closed++;
}

static void channel_recv_close(channel<int> c, int &closed) {
    try {
        c.recv();
    } catch (channel_closed_error &e) {
        closed++;
    }
}

void close_test_task(int &closed) {
    channel<int> c;
    taskspawn(std::bind(channel_recv_close, c, std::ref(closed)));
    taskspawn(std::bind(channel_recv_close, c, std::ref(closed)));
    taskspawn(std::bind(channel_closer_task, c, std::ref(closed)));
}

BOOST_AUTO_TEST_CASE(channel_close_test) {
    procmain p;
    int closed=0;
    taskspawn(std::bind(close_test_task, std::ref(closed)));
    p.main();
    BOOST_CHECK_EQUAL(closed, 3);
}

