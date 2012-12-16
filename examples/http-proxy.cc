#include "ten/task/compat.hh"
#include "ten/buffer.hh"
#include "ten/logging.hh"
#include "ten/net.hh"
#include "ten/channel.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"
#include <boost/lexical_cast.hpp>

using namespace ten;
using namespace ten::compat;

#define SEC2MS(s) (s*1000)

void sock_copy(channel<int> c, netsock &a, netsock &b, buffer &buf) {
    for (;;) {
        buf.reserve(4*1024);
        ssize_t nr = a.recv(buf.back(), buf.available());
        if (nr <= 0) break;
        buf.commit(nr);
        ssize_t nw = b.send(buf.front(), buf.size());
        buf.remove(nw);
    }
    DVLOG(3) << "shutting down sock_copy: " << a.s.fd << " to " << b.s.fd;
    shutdown(b.s.fd, SHUT_WR);
    a.close();
    c.send(1);
}

void send_503_reply(netsock &s) {
    http_response resp{503};
    std::string data = resp.data();
    ssize_t nw = s.send(data.data(), data.size());
    (void)nw; // ignore
}

void proxy_task(int sock) {
    netsock s{sock};
    buffer buf{4*1024};
    http_parser parser;
    http_request req;
    req.parser_init(&parser);

    bool got_headers = false;
    for (;;) {
        buf.reserve(4*1024);
        ssize_t nr = s.recv(buf.back(), buf.available(), SEC2MS(5));
        if (nr < 0) { goto request_read_error; }
        buf.commit(nr);
        size_t len = buf.size();
        req.parse(&parser, buf.front(), len);
        buf.remove(buf.size()); // normally would remove(len)
        if (req.complete) break;
        if (nr == 0) return;
        if (!got_headers && !req.method.empty()) {
            got_headers = true;
            if (req.get("Expect") == "100-continue") {
                http_response cont_resp(100);
                std::string data = cont_resp.data();
                ssize_t nw = s.send(data.data(), data.size());
                (void)nw;
            }
        }
    }

    try {
        uri u{req.uri};
        u.normalize();
        LOG(INFO) << req.method << " " << u.compose();

        netsock cs{AF_INET, SOCK_STREAM};

        if (req.method == "CONNECT") {
            ssize_t pos = u.path.find(':');
            u.host = u.path.substr(1, pos-1);
            u.port = boost::lexical_cast<uint16_t>(u.path.substr(pos+1));
            if (cs.dial(u.host.c_str(), u.port, SEC2MS(10))) {
                goto request_connect_error;
            }

            http_response resp{200};
            std::string data = resp.data();
            ssize_t nw = s.send(data.data(), data.size(), SEC2MS(5));

            channel<int> c;
            taskspawn(std::bind(sock_copy, c, std::ref(s), std::ref(cs), std::ref(buf)));
            taskspawn(std::bind(sock_copy, c, std::ref(cs), std::ref(s), std::ref(buf)));
            c.recv();
            c.recv();
            return;
        } else {
            if (u.port == 0) u.port = 80;
            if (cs.dial(u.host.c_str(), u.port, SEC2MS(10))) {
                goto request_connect_error;
            }

            http_request r{req.method, u.compose_path()};
            // HTTP/1.1 requires host header
            r.append("Host", u.host);
            r.headers = req.headers;
            std::string data = r.data();
            ssize_t nw = cs.send(data.data(), data.size(), SEC2MS(5));
            if (nw <= 0) { goto request_send_error; }
            if (!req.body.empty()) {
                nw = cs.send(req.body.data(), req.body.size(), SEC2MS(5));
                if (nw <= 0) { goto request_send_error; }
            }

            http_response resp{&r};
            resp.parser_init(&parser);
            bool headers_sent = false;
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = cs.recv(buf.back(), buf.available(), SEC2MS(5));
                if (nr < 0) { goto response_read_error; }
                buf.commit(nr);
                size_t len = buf.size();
                resp.parse(&parser, buf.front(), len);
                buf.remove(buf.size()); // normally would remove(len)
                if (headers_sent == false && resp.status_code) {
                    headers_sent = true;
                    data = resp.data();
                    nw = s.send(data.data(), data.size());
                    if (nw <= 0) { goto response_send_error; }
                }

                if (resp.body.size()) {
                    if (resp.get("Transfer-Encoding") == "chunked") {
                        char lenbuf[64];
                        int len = snprintf(lenbuf, sizeof(lenbuf)-1, "%zx\r\n", resp.body.size());
                        resp.body.insert(0, lenbuf, len);
                        resp.body.append("\r\n");
                    }
                    nw = s.send(resp.body.data(), resp.body.size());
                    if (nw <= 0) { goto response_send_error; }
                    resp.body.clear();
                }
                if (resp.complete) {
                    // send end chunk
                    if (resp.get("Transfer-Encoding") == "chunked") {
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
    netsock s{AF_INET, SOCK_STREAM};
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    address addr{"0.0.0.0", 3080};
    s.bind(addr);
    s.getsockname(addr);
    LOG(INFO) << "listening on: " << addr;
    s.listen();

    for (;;) {
        address client_addr;
        int sock;
        while ((sock = s.accept(client_addr, 0)) > 0) {
            taskspawn(std::bind(proxy_task, sock), 4*1024*1024);
        }
    }
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(listen_task);
    return p.main(argc, argv);
}
