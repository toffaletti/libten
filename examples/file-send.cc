#include "runner.hh"
#include "task.hh"
#include "channel.hh"
#include "buffer.hh"
#include <boost/bind.hpp>
#include <iostream>

// sends a file over a tcp socket on port 5500
// $ nc -l -p 5500 > out & ./file-send libfw.a
// $ md5sum out libfw.a                                                                                                                                                                                                
// 30faaa8f73d753d422ef3d0646a38297  out
// 30faaa8f73d753d422ef3d0646a38297  libfw.a

#define channel_cap 0
// need two 4096 byte blocks more than the channel cap
// because we'll be writing into the next one while
// file_sender is sending from the one it recv'ed from the channel
#define buffer_cap channel_cap+2

void file_reader(channel<buffer::slice> c, file_fd &f) {
    // circular buffer for reading/writing blocks
    // buffer is reference counted so it will be freed once
    // the channel is freed and all references to slices are removed
    buffer buf(buffer_cap*4096);
    int bufp = 0;
    for (;;) {
        // create a 4096 byte slice of the buffer
        buffer::slice bs = buf(bufp*4096, 4096);
        // read block from file
        ssize_t nr = f.read(bs, bs.size());
        if (nr <= 0) break;
        bs.resize(nr);
        // send block on channel to file_sender task
        c.send(bs);
        // move positions in the buffer to the next 4096 byte block
        bufp++;
        if (bufp == buffer_cap) bufp = 0;
    }
    buffer::slice empty;
    c.send(empty);
    c.close();
}

void file_sender(channel<buffer::slice> c) {
    address addr("127.0.0.1", 5500);
    task::socket s(AF_INET, SOCK_STREAM);
    s.connect(addr, 100);
    size_t bytes_sent = 0;
    for (;;) {
        buffer::slice bs = c.recv();
        if (bs.size() == 0) break;
        ssize_t nw = s.send(bs, bs.size());
        if (nw > 0) bytes_sent += nw;
    }
    std::cout << "sent " << bytes_sent << " bytes\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    runner::init();
    file_fd f(argv[1], 0, 0);
    channel<buffer::slice> c(channel_cap);
    task::spawn(boost::bind(file_sender, c));
    // start a new OS-thread for file_reader
    runner::spawn(boost::bind(file_reader, c, boost::ref(f)));
    return runner::main();
}
