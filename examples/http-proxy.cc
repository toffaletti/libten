#include "ten/task.hh"
#include "ten/buffer.hh"
#include "ten/logging.hh"
#include "ten/net.hh"
#include "ten/channel.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"
#include <boost/lexical_cast.hpp>

using namespace ten;
const size_t default_stacksize=256*1024;

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
    using namespace std::chrono;
    netsock s{sock};
    buffer buf{4*1024};
    http_parser parser;
    http_request req;
    req.parser_init(&parser);

    bool got_headers = false;
    for (;;) {
        buf.reserve(4*1024);
        ssize_t nr = s.recv(buf.back(), buf.available(), 0, duration_cast<milliseconds>(seconds{5}));
        if (nr < 0) { goto request_read_error; }
        buf.commit(nr);
        size_t len = buf.size();
        req.parse(&parser, buf.front(), len);
        buf.remove(buf.size()); // normally would remove(len)
        if (req.complete) break;
        if (nr == 0) return;
        if (!got_headers && !req.method.empty()) {
            got_headers = true;
            auto exp_hdr = req.get("Expect");
            if (exp_hdr && *exp_hdr == "100-continue") {
                http_response cont_resp(100);
                std::string data = cont_resp.data();
                ssize_t nw = s.send(data.data(), data.size());
                (void)nw;
            }
        }
    }

    try {
        using namespace std::chrono;
        uri u{req.uri};
        u.normalize();
        LOG(INFO) << req.method << " " << u.compose();

        netsock cs{AF_INET, SOCK_STREAM};

        if (req.method == "CONNECT") {
            ssize_t pos = u.path.find(':');
            u.host = u.path.substr(1, pos-1);
            u.port = boost::lexical_cast<uint16_t>(u.path.substr(pos+1));
            cs.dial(u.host.c_str(), u.port, duration_cast<milliseconds>(seconds{10}));

            http_response resp{200};
            std::string data = resp.data();
            ssize_t nw = s.send(data.data(), data.size(), 0, duration_cast<milliseconds>(seconds{5}));

            channel<int> c;
            task::spawn([&] {
                sock_copy(c, s, cs, buf);
            });
            task::spawn([&] {
                sock_copy(c, cs, s, buf);
            });
            // wait for sock copy tasks to exit
            c.recv();
            c.recv();
            return;
        } else {
            if (u.port == 0) u.port = 80;
            cs.dial(u.host.c_str(), u.port, duration_cast<milliseconds>(seconds{10}));

            http_request r{req};
            r.uri = u.compose_path();
            // HTTP/1.1 requires host header
            r.set("Host", u.host);
            std::string data = r.data();
            ssize_t nw = cs.send(data.data(), data.size(), 0, duration_cast<milliseconds>(seconds{5}));
            if (nw <= 0) { goto request_send_error; }
            if (!req.body.empty()) {
                nw = cs.send(req.body.data(), req.body.size(), 0, duration_cast<milliseconds>(seconds{5}));
                if (nw <= 0) { goto request_send_error; }
            }

            http_response resp{&r};
            resp.parser_init(&parser);
            bool headers_sent = false;
            optional<std::string> tx_enc_hdr;
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = cs.recv(buf.back(), buf.available(), 0, duration_cast<milliseconds>(seconds{5}));
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
                    if (tx_enc_hdr && *tx_enc_hdr == "chunked") {
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
                    tx_enc_hdr = resp.get("Transfer-Encoding");
                    // send end chunk
                    if (tx_enc_hdr && *tx_enc_hdr == "chunked") {
                        nw = s.send("0\r\n\r\n", 5);
                    }
                    break;
                }
                if (nr == 0) { goto response_read_error; }
            }
            return;
        }
    } catch (errno_error &e) {
        PLOG(ERROR) << "request connect error " << req.method << " " << req.uri;
        send_503_reply(s);
        return;
    } catch (std::exception &e) {
        LOG(ERROR) << "exception error: " << req.uri << " : " << e.what();
        return;
    }
request_read_error:
    PLOG(ERROR) << "request read error";
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

int main(int argc, char *argv[]) {
    kernel::boot();
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
            task::spawn([=] {
                proxy_task(sock);
            });
        }
    }
}
