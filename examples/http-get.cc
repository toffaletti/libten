#include "ten/task/compat.hh"
#include "ten/buffer.hh"
#include "ten/net.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"

#include <iostream>

using namespace ten;
using namespace ten::compat;

static void do_get(uri u) {
    netsock s{AF_INET, SOCK_STREAM};
    u.normalize();
    if (u.scheme != "http") return;
    if (u.port == 0) u.port = 80;
    s.dial(u.host.c_str(), u.port);

    http_request r{"GET", u.compose_path()};
    // HTTP/1.1 requires host header
    r.append("Host", u.host); 

    std::string data = r.data();
    std::cout << "Request:\n" << "--------------\n";
    std::cout << data;
    ssize_t nw = s.send(data.c_str(), data.size());

    buffer buf{4*1024};

    http_parser parser;
    http_response resp;
    resp.parser_init(&parser);

    while (!resp.complete) {
        ssize_t nr = s.recv(buf.back(), buf.available());
        if (nr <= 0) { std::cerr << "Error: " << strerror(errno) << "\n"; break; }
        buf.commit(nr);
        size_t len = buf.size();
        resp.parse(&parser, buf.front(), len);
        buf.remove(len);
    }

    std::cout << "Response:\n" << "--------------\n";
    std::cout << resp.data();
    std::cout << "Body size: " << resp.body.size() << "\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) return -1;
    procmain p;

    uri u{argv[1]};
    taskspawn(std::bind(do_get, u));
    return p.main(argc, argv);
}
