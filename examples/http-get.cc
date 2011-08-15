#include "runner.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/http_message.hh"

#include <iostream>

using namespace fw;

static void do_get() {
    task::socket s(AF_INET, SOCK_STREAM);
    s.dial("www.google.com", 80);

    http_request r("GET", "/");
    std::string data = r.data();
    ssize_t nw = s.send(data.c_str(), data.size());

    buffer buf(4*1024*1024);
    buffer::slice rb = buf(0);

    http_response_parser p;
    memset(&p, 0, sizeof(p));
    http_response resp;

    ssize_t pos = 0;
    while (!http_response_parser_is_finished(&p)) {
        resp.parser_init(&p);
        ssize_t nr = s.recv(rb.data(pos), rb.size()-pos);
        if (nr <= 0) break;
        http_response_parser_execute(&p, rb.data(), pos+nr, 0);
        pos += nr;
        if (http_response_parser_has_error(&p)) break;

        if (!http_response_parser_is_finished(&p)) {
            resp.clear();
        }
    }

    if (http_response_parser_is_finished(&p) && !http_response_parser_has_error(&p)) {
        std::cout << resp.data();
    }

    if (resp.header_string("Transfer-Encoding") == "chunked") {
    }
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(do_get, 0, 4*1024*1024);
    return runner::main();
}
