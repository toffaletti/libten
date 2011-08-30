#include "runner.hh"
#include "task.hh"
#include "buffer.hh"
#include "http/http_message.hh"
#include "uri/uri.hh"
#include "logging.hh"
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

using namespace fw;

#define SEC2MS(s) (s*1000)

void sock_copy(task::socket &a, task::socket &b, buffer::slice rb) {
    for (;;) {
        pollfd fds[2];
        fds[0].events = EPOLLIN;
        fds[0].fd = a.s.fd;
        fds[1].events = EPOLLIN;
        fds[1].fd = b.s.fd;
        if (!task::poll(fds, 2, 0)) break;
        if (fds[0].revents) {
            ssize_t nr = a.recv(rb.data(), rb.size());
            if (nr <= 0) break;
            ssize_t nw = b.send(rb.data(), nr);
            if (nw != nr) break;
        }
        if (fds[1].revents) {
            ssize_t nr = b.recv(rb.data(), rb.size());
            if (nr <= 0) break;
            ssize_t nw = a.send(rb.data(), nr);
            if (nw != nr) break;

        }
    }
}

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
        LOG(INFO) << req.method << " " << u.compose();

        task::socket cs(AF_INET, SOCK_STREAM);

        if (req.method == "CONNECT") {
            ssize_t pos = u.path.find(':');
            u.host = u.path.substr(1, pos-1);
            u.port = boost::lexical_cast<uint16_t>(u.path.substr(pos+1));
            if (cs.dial(u.host.c_str(), u.port, SEC2MS(10))) {
                goto request_connect_error;
            }

            http_response resp(200, "Connected ok");
            std::string data = resp.data();
            ssize_t nw = s.send(data.data(), data.size(), SEC2MS(5));

            sock_copy(s, cs, rb);
            return;
        } else {
            if (u.port == 0) u.port = 80;
            if (cs.dial(u.host.c_str(), u.port, SEC2MS(10))) {
                goto request_connect_error;
            }

            http_request r(req.method, u.compose(true));
            // HTTP/1.1 requires host header
            r.append_header("Host", u.host);
            r.headers = req.headers;
            std::string data = r.data();
            ssize_t nw = cs.send(data.data(), data.size(), SEC2MS(5));
            if (nw <= 0) { goto request_send_error; }
            if (!req.body.empty()) {
                nw = cs.send(req.body.data(), req.body.size(), SEC2MS(5));
                if (nw <= 0) { goto request_send_error; }
            }

            http_response resp(&r);
            resp.parser_init(&parser);
            bool headers_sent = false;
            for (;;) {
                ssize_t nr = cs.recv(rb.data(), rb.size(), SEC2MS(5));
                if (nr < 0) { goto response_read_error; }
                bool complete = resp.parse(&parser, rb.data(), nr);
                if (headers_sent == false && resp.status_code) {
                    headers_sent = true;
                    data = resp.data();
                    nw = s.send(data.data(), data.size());
                    if (nw <= 0) { goto response_send_error; }
                }

                if (resp.body.size()) {
                    if (resp.header_string("Transfer-Encoding") == "chunked") {
                        char lenbuf[64];
                        int len = snprintf(lenbuf, sizeof(lenbuf)-1, "%zx\r\n", resp.body.size());
                        resp.body.insert(0, lenbuf, len);
                        resp.body.append("\r\n");
                    }
                    nw = s.send(resp.body.data(), resp.body.size());
                    if (nw <= 0) { goto response_send_error; }
                    resp.body.clear();
                }
                if (complete) {
                    // send end chunk
                    if (resp.header_string("Transfer-Encoding") == "chunked") {
                        nw = s.send("0\r\n\r\n", 5);
                    }
                    break;
                }
                if (nr == 0) { goto response_read_error; }
            }
            return;
        }
    } catch (std::exception &e) {
        LOG(ERROR) << "exception error: " << req.uri << " : " << e.what();
        return;
    }
request_read_error:
    PLOG(ERROR) << "request read error";
    return;
request_connect_error:
    PLOG(ERROR) << "request connect error " << req.method << " " << req.uri;
    send_503_reply(s);
    return;
request_send_error:
    PLOG(ERROR) << "request send error: " << req.method << " " << req.uri;
    send_503_reply(s);
    return;
response_read_error:
    PLOG(ERROR) << "response read error: " << req.method << " " << req.uri;
    send_503_reply(s);
    return;
response_send_error:
    PLOG(ERROR) << "response send error: " << req.method << " " << req.uri;
    return;
}

void listen_task() {
    task::socket s(AF_INET, SOCK_STREAM);
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr("0.0.0.0", 8080);
    s.bind(addr);
    s.getsockname(addr);
    LOG(INFO) << "listening on: " << addr;
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
