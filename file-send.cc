#include "runner.hh"
#include "task.hh"
#include "channel.hh"
#include <boost/bind.hpp>

// sends a file over a tcp socket on port 5500
// $ nc -l -p 5500 > out & ./file-send libfw.a
// $ md5sum out libfw.a                                                                                                                                                                                                
// 30faaa8f73d753d422ef3d0646a38297  out
// 30faaa8f73d753d422ef3d0646a38297  libfw.a

struct buffer {
    char *buf;
    size_t len;
};

#define channel_cap 0
// need two buffers more than the channel cap
// because we'll be writing into the next one while
// file_sender is sending from one
#define buffer_cap channel_cap+2

void file_reader(channel<buffer> c, file_fd &f) {
    // use two buffers because we'll hand one off to file_sender
    // while we read into the next one
    buffer bufs[buffer_cap];
    // allocate buffers on the heap so they will still
    // exist for file_sender after this task has exited
    for (int i=0; i<buffer_cap; ++i) {
        bufs[i].buf = new char[4096];
    }
    int bufp = 0;
    for (;;) {
        // read block from file
        ssize_t nr = f.read(bufs[bufp].buf, 4096);
        if (nr <= 0) break;
        // send block on channel
        bufs[bufp].len = nr;
        c.send(bufs[bufp]);
        // use the other buffer while file_sender is
        // busy with the one we just sent
        bufp++;
        if (bufp == buffer_cap) bufp = 0;
    }
    buffer empty = {0,0};
    c.send(empty);
    c.close();
}

void file_sender(channel<buffer> c) {
    address addr("127.0.0.1", 5500);
    task::socket s(AF_INET, SOCK_STREAM);
    s.connect(addr, 100);
    for (;;) {
        buffer buf = c.recv();
        if (buf.len == 0) break;
        ssize_t nw = s.send(buf.buf, buf.len);
        assert(nw == buf.len);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    file_fd f(argv[1], 0, 0);
    channel<buffer> c(channel_cap);
    task::spawn(boost::bind(file_sender, c));
    // start a new OS-thread for file_reader
    runner::spawn(boost::bind(file_reader, c, boost::ref(f)));
    runner::self().schedule();
    return 0;
}