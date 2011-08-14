#define BOOST_TEST_MODULE channel test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>
#include "runner.hh"
#include "descriptors.hh"
#include "semaphore.hh"
#include "channel.hh"

using namespace fw;

static void channel_recv(channel<intptr_t> c) {
    intptr_t d = c.recv();
    BOOST_CHECK_EQUAL(d, 1875309);
    char *str = reinterpret_cast<char *>(c.recv());
    BOOST_CHECK_EQUAL(str, "hi");
}

static void channel_send(channel<intptr_t> c) {
    static char str[] = "hi";
    c.send(1875309);
    c.send(reinterpret_cast<intptr_t>(str));
}

BOOST_AUTO_TEST_CASE(channel_test) {
    runner::init();
    channel<intptr_t> c(10);
    task::spawn(boost::bind(channel_recv, c));
    task::spawn(boost::bind(channel_send, c));
    runner::main();
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_test) {
    runner::init();
    channel<intptr_t> c;
    task::spawn(boost::bind(channel_recv, c));
    task::spawn(boost::bind(channel_send, c));
    runner::main();
    BOOST_CHECK(c.empty());
}

static void channel_recv_mt(channel<intptr_t> c, semaphore &s) {
    intptr_t d = c.recv();
    assert(d == 1875309);
    char *str = reinterpret_cast<char *>(c.recv());
    assert(strcmp(str, "hi") == 0);
    s.post();
}

BOOST_AUTO_TEST_CASE(channel_unbuffered_mt_test) {
    runner::init();
    semaphore s;
    channel<intptr_t> c;
    runner::spawn(boost::bind(channel_recv_mt, c, boost::ref(s)));
    runner::spawn(boost::bind(channel_send, c));
    s.wait();
    BOOST_CHECK(c.empty());
}

static void channel_multi_send(channel<intptr_t> c) {
    c.send(1234);
    c.send(1234);
}

static void channel_multi_recv(channel<intptr_t> c) {
    for (int i=0; i<5; ++i) {
        intptr_t d = c.recv();
        BOOST_CHECK_EQUAL(d, 1234);
    }
    BOOST_CHECK(c.empty());
}

BOOST_AUTO_TEST_CASE(channel_multiple_senders_test) {
    runner::init();
    channel<intptr_t> c(4);
    c.send(1234);
    task::spawn(boost::bind(channel_multi_recv, c));
    task::spawn(boost::bind(channel_multi_send, c));
    task::spawn(boost::bind(channel_multi_send, c));
    runner::main();
    BOOST_CHECK(c.empty());
}

static void delayed_channel_send(channel<int> c) {
    task::sleep(100);
    c.send(5309);
}

static void delayed_channel(address addr) {
    channel<int> c;
    // spawn the send in another thread before blocking on recv
    runner::spawn(boost::bind(delayed_channel_send, c));
    int a = c.recv();

    socket_fd s(AF_INET, SOCK_STREAM);
    s.setnonblock();
    if (s.connect(addr) == 0) {
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        bool success = task::poll(s.fd, EPOLLOUT);
        assert(success);
    } else {
        throw errno_error();
    }
}

static void wait_on_io() {
    socket_fd s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("127.0.0.1", 0);
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    task::spawn(boost::bind(delayed_channel, addr));
    task::poll(s.fd, EPOLLIN);
}

BOOST_AUTO_TEST_CASE(blocked_io_and_channel) {
    runner::init();
    task::spawn(wait_on_io);
    runner::main();
}

static void channel_closer(channel<int> c, int &closed) {
    task::yield();
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

BOOST_AUTO_TEST_CASE(channel_close_test) {
    runner::init();
    channel<int> c;
    int closed=0;
    task::spawn(boost::bind(channel_recv_close, c, boost::ref(closed)));
    task::spawn(boost::bind(channel_recv_close, c, boost::ref(closed)));
    task::spawn(boost::bind(channel_closer, c, boost::ref(closed)));
    runner::main();
    BOOST_CHECK_EQUAL(closed, 3);
}
