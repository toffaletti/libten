#include "runner.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/http_message.hh"
#include "uri/uri.hh"
#include <boost/bind.hpp>

#include <iostream>

using namespace fw;

static void do_get(uri u) {
    task::socket s(AF_INET, SOCK_STREAM);
    u.normalize();
    s.dial(u.host.c_str(), 80);

    http_request r("GET", u.compose(true));
    // HTTP/1.1 requires host header
    r.append_header("Host", u.host);
    std::string data = r.data();
    std::cout << "Request:\n" << "--------------\n";
    std::cout << data;
    ssize_t nw = s.send(data.c_str(), data.size());

    buffer buf(4*1024*1024);
    buffer::slice rb = buf(0);

    http_response resp;

    ssize_t pos = 0;
    for (;;) {
        ssize_t nr = s.recv(rb.data(pos), rb.size()-pos);
        if (nr <= 0) break;
        pos += nr;
        if (resp.parse(rb.data(), pos)) break;
    }

    std::cout << "Response:\n" << "--------------\n";
    std::cout << resp.data();

    if (resp.header_string("Transfer-Encoding") == "chunked") {
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) return -1;
    runner::init();

    uri u(argv[1]);
    task::spawn(boost::bind(do_get, u), 0, 4*1024*1024);
    return runner::main();
}
