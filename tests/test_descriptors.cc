#include "gtest/gtest.h"
#include "ten/descriptors.hh"

using namespace ten;

TEST(DescriptorTest, MoveTest) {
    fd_base fd1{0};
    fd_base fd2{std::move(fd1)};
    EXPECT_EQ(fd1.fd, -1);
    EXPECT_EQ(fd2.fd, 0);
}

TEST(DescriptorTest, SocketPairCXX11) {
    auto sp = socket_fd::pair(AF_UNIX, SOCK_STREAM);
    {
        char a = 1;
        EXPECT_EQ(sp.first.write(&a, 1), 1);
        EXPECT_EQ(sp.second.read(&a, 1), 1);
        EXPECT_EQ(a, 1);
    }
}

TEST(DescriptorTest, SocketPair) {
    int sv[2];
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    socket_fd sp1{sv[0]};
    socket_fd sp2{sv[1]};
    char a = 1;
    EXPECT_EQ(sp1.write(&a, 1), 1);
    EXPECT_EQ(sp2.read(&a, 1), 1);
    EXPECT_EQ(a, 1);
}

TEST(DescriptorTest, pipe_test) {
    char a = 1;
    pipe_fd pipe;
    EXPECT_EQ(pipe.write(&a, 1), 1);
    EXPECT_EQ(pipe.read(&a, 1), 1);
    EXPECT_EQ(a, 1);
}


TEST(DescriptorTest, timer_fd_test) {
    timer_fd t;
    itimerspec ts;
    itimerspec prev;
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec = 1;
    ts.it_value.tv_nsec = 50;
    t.settime(ts, prev);
}

TEST(DescriptorTest, socket_listen_test) {
    socket_fd s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"0.0.0.0", 0};
    s.bind(addr);
    s.listen();
}

TEST(DescriptorTest, udp_socket_test) {
    socket_fd us{AF_INET, SOCK_DGRAM};
    us.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address uaddr{"0.0.0.0", 0};
    us.bind(uaddr);
}

TEST(DescriptorTest, signal_fd_test) {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    signal_fd sig{sigset};
}

TEST(DescriptorTest, event_fd_test) {
    event_fd efd;
    efd.write(1);
    EXPECT_EQ(efd.read(), 1u);
    efd.write(1);
    efd.write(2);
    efd.write(3);
    EXPECT_EQ(efd.read(), (unsigned)(1+2+3));
}
