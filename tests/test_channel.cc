#include "gtest/gtest.h"
#include "ten/descriptors.hh"
#include "ten/semaphore.hh"
#include "ten/channel.hh"

using namespace ten;

static void channel_recv(channel<intptr_t> c, channel<int> done_chan) {
    intptr_t d = c.recv();
    EXPECT_EQ(d, 8675309);
    char *str = reinterpret_cast<char *>(c.recv());
    EXPECT_STREQ(str, "hi");
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
    task::spawn([=] {
        channel_recv(c, done_chan);
    });
    task::spawn([=] {
        channel_send(c, done_chan);
    });
    for (int i=0; i<2; ++i) {
        done_chan.recv();
    }
    EXPECT_TRUE(c.empty());
}

TEST(Channel, Basic) {
    task::main([] {
        task::spawn(channel_test_task);
    });
}

void unbuffered_test_task() {
    channel<int> done_chan;
    channel<intptr_t> c;
    task::spawn([=] {
        channel_recv(c, done_chan);
    });
    task::spawn([=] {
        channel_send(c, done_chan);
    });
    for (int i=0; i<2; ++i) {
        done_chan.recv();
    }
    EXPECT_TRUE(c.empty());
}

TEST(Channel, Unbuffered) {
    task::main([] {
        task::spawn(unbuffered_test_task);
    });
}

void mt_test_task() {
    channel<intptr_t> c;
    channel<int> done_chan;
    auto recv_thread = task::spawn_thread([=] {
        channel_recv(c, done_chan);
    });
    auto send_thread = task::spawn_thread([=] {
        channel_send(c, done_chan);
    });
    for (int i=0; i<2; ++i) {
        done_chan.recv();
    }
    recv_thread.join();
    send_thread.join();
    EXPECT_TRUE(c.empty());
}

TEST(Channel, UnbufferedThreaded) {
    task::main([] {
        task::spawn(mt_test_task);
    });
}

static void channel_multi_send(channel<intptr_t> c, channel<int> done_chan) {
    c.send(1234);
    c.send(1234);
    done_chan.send(1);
}

static void channel_multi_recv(channel<intptr_t> c, channel<int> done_chan) {
    for (int i=0; i<5; ++i) {
        intptr_t d = c.recv();
        EXPECT_EQ(d, 1234);
    }
    EXPECT_TRUE(c.empty());
    done_chan.send(1);
}

void multiple_senders_task() {
    channel<int> done_chan;
    channel<intptr_t> c{4};
    c.send(1234);
    task::spawn([=] {
        channel_multi_recv(c, done_chan);
    });
    task::spawn([=] {
        channel_multi_send(c, done_chan);
    });
    task::spawn([=] {
        channel_multi_send(c, done_chan);
    });
    for (int i=0; i<3; ++i) {
        done_chan.recv();
    }
    EXPECT_TRUE(c.empty());
}

TEST(Channel, MultipleSenders) {
    task::main([]{
        task::spawn(multiple_senders_task);
    });
}

static void delayed_channel_send(channel<int> c) {
    using namespace std::chrono;
    this_task::sleep_for(milliseconds{100});
    c.send(5309);
}

static void delayed_channel(address addr) {
    channel<int> c;
    // spawn the send in another thread before blocking on recv
    auto channel_send_thread = task::spawn_thread([=] {
        delayed_channel_send(c);
    });
    int a = c.recv();
    (void)a;

    socket_fd s{AF_INET, SOCK_STREAM};
    s.setnonblock();
    if (s.connect(addr) == 0) {
    } else if (errno == EINPROGRESS) {
        // poll for writeable
        EXPECT_TRUE(fdwait(s.fd, 'w'));
    } else {
        throw errno_error();
    }
    channel_send_thread.join();
}

static void wait_on_io() {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"127.0.0.1", 0};
    s.bind(addr);
    s.getsockname(addr);
    s.listen();

    task::spawn([=] {
        delayed_channel(addr);
    });
    fdwait(s.fd, 'r');
    // yield here, otherwise fdwait in delayed_channel
    // could get the close event on our listening socket
    this_task::yield();
}

TEST(Channel, BlockedIoAndChannel) {
    task::main([]{
        task::spawn(wait_on_io);
    });
}

static void channel_closer_task(channel<int> c, int &closed) {
    this_task::yield();
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
    task::spawn([=, &closed] {
        channel_recv_close(c, closed);
    });
    task::spawn([=, &closed] {
        channel_recv_close(c, closed);
    });
    task::spawn([=, &closed] {
        channel_closer_task(c, closed);
    });
}

TEST(Channel, Close) {
    int closed=0;
    task::main([&] {
        task::spawn([&] {
            close_test_task(closed);
        });
    });
    EXPECT_EQ(closed, 3);
}

