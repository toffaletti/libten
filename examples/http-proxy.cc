#include "runner.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/http_message.hh"
#include "uri/uri.hh"
#include <boost/bind.hpp>

#include <iostream>

using namespace fw;

#define SEC2MS(s) (s*1000)

void send_503_reply(task::socket &s) {
    http_response resp(503, "Gateway Timeout");
    std::string data = resp.data();
    ssize_t nw = s.send(data.data(), data.size());
    (void)nw; // ignore
}

void proxy_task(int sock) {
    task::socket s(sock);
    buffer buf(4*1024);
    http_parser parser;
    http_request req;
    req.parser_init(&parser);

    buffer::slice rb = buf(0);
    for (;;) {
        ssize_t nr = s.recv(rb.data(), rb.size(), SEC2MS(5));
        if (nr < 0) { goto request_read_error; }
        if (req.parse(&parser, rb.data(), nr)) break;
        if (nr == 0) return;
    }

    try {
        uri u(req.uri);
        u.normalize();
        std::cout << req.method << " " << u.compose() << "\n";

        task::socket cs(AF_INET, SOCK_STREAM);
        if (u.port == 0) u.port = 80;
        if (cs.dial(u.host.c_str(), u.port)) {
            goto request_connect_error;
        }

        http_request r(req.method, u.compose(true));
        // HTTP/1.1 requires host header
        r.append_header("Host", u.host);
        r.headers = req.headers;
        std::string data = r.data();
        ssize_t nw = cs.send(data.data(), data.size());
        if (nw <= 0) { goto request_send_error; }
        if (!req.body.empty()) {
            nw = cs.send(req.body.data(), req.body.size());
            if (nw <= 0) { goto request_send_error; }
        }

        http_response resp;
        resp.parser_init(&parser);
        for (;;) {
            ssize_t nr = cs.recv(rb.data(), rb.size(), SEC2MS(5));
            if (nr < 0) { goto response_read_error; }
            if (resp.parse(&parser, rb.data(), nr)) break;
            if (nr == 0) { goto response_read_error; }
        }

        // remove "chunked" encoding
        resp.remove_header("Transfer-Encoding");
        resp.remove_header("Content-Length");
        resp.append_header("Content-Length", resp.body.size());

        data = resp.data();
        nw = s.send(data.data(), data.size());
        if (nw <= 0) { goto response_send_error; }

        if (!resp.body.empty()) {
            nw = s.send(resp.body.data(), resp.body.size());
            if (nw <= 0) { goto response_send_error; }
        }
        return;
    } catch (std::exception &e) {
        std::cerr << "exception error: " << req.uri << " : " << e.what() << "\n";
        return;
    }
request_read_error:
    std::cerr << "request read error: " << errno << ": " << strerror(errno) << "\n";
    return;
request_connect_error:
    std::cerr << "request connect error: " << errno << ": " << strerror(errno) << "\n";
    send_503_reply(s);
    return;
request_send_error:
    std::cerr << "request send error: " << errno << ": " << strerror(errno) << "\n";
    send_503_reply(s);
    return;
response_read_error:
    std::cerr << "response read error: " << errno << ": " << strerror(errno) << "\n";
    send_503_reply(s);
    return;
response_send_error:
    std::cerr << "response send error: " << errno << ": " << strerror(errno) << "\n";
    return;
}

void listen_task() {
    task::socket s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 8080);
    s.bind(addr);
    s.getsockname(addr);
    std::cout << "listening on: " << addr << "\n";
    s.listen();

    for (;;) {
        address client_addr;
        int sock;
        while ((sock = s.accept(client_addr, 0)) > 0) {
            task::spawn(boost::bind(proxy_task, sock), 0, 4*1024*1024);
        }
    }
}

int main(int argc, char *argv[]) {
    runner::init();
    task::spawn(listen_task);
    return runner::main();
}
