#include "ten/task.hh"
#include "ten/buffer.hh"
#include "ten/net/ssl.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"

#include <iostream>

using namespace ten;
const size_t default_stacksize=256*1024;

static void do_get(uri u) {
    sslsock s{AF_INET, SOCK_STREAM};
    s.initssl(SSLv23_client_method(), true);
    u.normalize();
    if (u.scheme != "https") return;
    if (u.port == 0) u.port = 443;
    s.dial(u.host.c_str(), u.port);

    http_request r{"GET", u.compose(true)};
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
    SSL_load_error_strings();
    SSL_library_init();

    uri u{argv[1]};
    taskspawn(std::bind(do_get, u));
    return p.main(argc, argv);
}
