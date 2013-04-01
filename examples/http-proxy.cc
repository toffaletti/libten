#include "ten/task.hh"
#include "ten/buffer.hh"
#include "ten/logging.hh"
#include "ten/net.hh"
#include "ten/channel.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"
#include <boost/lexical_cast.hpp>

using namespace ten;

static optional_timeout connect_timeout = nullopt;
static optional_timeout rw_timeout = nullopt;

void sock_copy(netsock &a, netsock &b) {
    buffer buf{4*1024};
    for (;;) {
        buf.reserve(4*1024);
        ssize_t nr = a.recv(buf.back(), buf.available(), 0, rw_timeout);
        if (nr <= 0) break;
        buf.commit(nr);
        ssize_t nw = b.send(buf.front(), buf.size(), 0, rw_timeout);
        if (nw <= 0) break;
        buf.remove(nw);
    }
    DVLOG(3) << "shutting down sock_copy: " << a.s.fd << " to " << b.s.fd;
    int s = b.shutdown(SHUT_WR);
    (void)s;
    a.close();
}

void send_503_reply(netsock &s) {
    http_response resp{503};
    std::string data = resp.data();
    ssize_t nw = s.send(data.data(), data.size(), 0, rw_timeout);
    (void)nw; // ignore
}

void proxy_task(int sock) {
    using namespace std::chrono;
    netsock s{sock};
    buffer buf{4*1024};
    http_parser parser;
    auto on_incoming_headers = [&](http_request &req) {
        req.body.reserve(req.body_length);
        auto exp_hdr = req.get("Expect");
        if (exp_hdr && *exp_hdr == "100-continue") {
            http_response cont_resp(100);
            std::string data = cont_resp.data();
            ssize_t nw = s.send(data.data(), data.size(), 0, rw_timeout);
            (void)nw;
        }
    };
    http_request req{on_incoming_headers};
    parser.init(req);
    for (;;) {
        buf.reserve(4*1024);
        ssize_t nr = s.recv(buf.back(), buf.available(), 0, rw_timeout);
        if (nr < 0) { goto request_read_error; }
        buf.commit(nr);
        size_t len = buf.size();
        parser(buf.front(), len);
        buf.remove(buf.size()); // normally would remove(len)
        if (parser.complete()) break;
        if (nr == 0) return;
    }

    try {
        using namespace std::chrono;
        LOG(INFO) << req.method << " " << req.uri;
        netsock cs{AF_INET, SOCK_STREAM};

        if (req.method == "CONNECT") {
            std::string host = req.uri;
            uint16_t port{443};
            parse_host_port(host, port);
            cs.dial(host.c_str(), port, connect_timeout);

            http_response resp{200};
            std::string data = resp.data();
            ssize_t nw = s.send(data.data(), data.size(), 0, rw_timeout);
            (void)nw;

            auto copy_task1 = task::spawn([&] {
                sock_copy(s, cs);
            });
            auto copy_task2 = task::spawn([&] {
                sock_copy(cs, s);
            });
            // wait for sock copy tasks to exit
            copy_task1.join();
            copy_task2.join();
            return;
        } else {
            uri u{req.uri};
            u.normalize();
            if (u.port == 0) u.port = 80;
            cs.dial(u.host.c_str(), u.port, connect_timeout);

            http_request r{req};
            r.uri = u.compose_path();
            // HTTP/1.1 requires host header
            r.set("Host", u.host);
            std::string data = r.data();
            ssize_t nw = cs.send(data.data(), data.size(), 0, rw_timeout);
            if (nw <= 0) { goto request_send_error; }
            if (!req.body.empty()) {
                nw = cs.send(req.body.data(), req.body.size(), 0, rw_timeout);
                if (nw <= 0) { goto request_send_error; }
            }

            bool chunked = false;
            auto on_headers = [&](const http_response &resp) {
                optional<std::string> tx_enc_hdr = resp.get("Transfer-Encoding");
                if (tx_enc_hdr && *tx_enc_hdr == "chunked") {
                    chunked = true;
                }
                data = resp.data();
                size_t nw = s.send(data.data(), data.size());
                if (nw <= 0) { throw errorx("response send error"); }
            };
            buffer wbuf{4096};
            auto on_content_part = [&](const http_response &resp, const char *part, size_t plen) {
                if (chunked) {
                    wbuf.reserve(64+plen+2);
                    int len = snprintf(wbuf.back(), wbuf.available(), "%zx\r\n", plen);
                    // TODO: should really use writev here
                    wbuf.commit(len);
                    wbuf.reserve(plen+2);
                    memcpy(wbuf.back(), part, plen);
                    wbuf.commit(plen);
                    memcpy(wbuf.back(), "\r\n", 2);
                    wbuf.commit(2);
                    nw = s.send(wbuf.front(), wbuf.size());
                    if (nw <= 0) { throw errorx("response send error"); }
                    wbuf.remove(nw);
                } else {
                    nw = s.send(part, plen);
                    if (nw <= 0) { throw errorx("response send error"); }
                }
            };

            http_response resp{&r, on_headers, on_content_part};
            parser.init(resp);
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = cs.recv(buf.back(), buf.available(), 0, rw_timeout);
                if (nr < 0) { goto response_read_error; }
                buf.commit(nr);
                size_t len = buf.size();
                parser(buf.front(), len);
                buf.remove(buf.size()); // normally would remove(len)
                if (parser.complete()) {
                    // send end chunk
                    if (chunked) {
                        nw = s.send("0\r\n\r\n", 5, 0, rw_timeout);
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
}

int main() {
    return task::main([] {
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
    });
}
