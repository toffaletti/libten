#ifndef HTTP_CLIENT_HH
#define HTTP_CLIENT_HH

#include "http/http_message.hh"
#include "shared_pool.hh"
#include "buffer.hh"

namespace ten {

class http_error : public errorx {
public:
    http_error(const std::string &msg) : errorx(msg) {}
};

class http_client {
private:
    std::shared_ptr<netsock> s;
    buffer buf;

    void ensure_connection() {
        if (s && s->valid()) return;
        s.reset(new netsock(AF_INET, SOCK_STREAM));
        if (s->dial(host.c_str(), port) != 0) {
            throw http_error("dial");
        }
    }

public:
    const std::string host;
    const uint16_t port;
    size_t max_content_length;

    http_client(const std::string &host_, uint16_t port_=80)
        : buf(4*1024), host(host_), port(port_), max_content_length(~(size_t)0) {}

    http_response perform(const std::string &method, const std::string &path, const std::string &data="") {
        uri u;
        u.scheme = "http";
        u.host = host;
        u.port = port;
        u.path = path;
        u.normalize();

        http_request r(method, u.compose_path());
        // HTTP/1.1 requires host header
        r.set("Host", u.host); 
        r.body = data;
        return perform(r);
    }

    http_response perform(http_request &r) {
        if (r.body.size()) {
            r.set("Content-Length", r.body.size());
        }

        try {
            ensure_connection();

            std::string hdata = r.data();
            ssize_t nw = s->send(hdata.c_str(), hdata.size());
            if (r.body.size()) {
                nw = s->send(r.body.c_str(), r.body.size());
            }

            http_parser parser;
            http_response resp;
            resp.parser_init(&parser);

            buf.clear();

            while (!resp.complete) {
                buf.reserve(4*1024);
                ssize_t nr = s->recv(buf.back(), buf.available());
                if (nr <= 0) { throw http_error("recv"); }
                buf.commit(nr);
                size_t len = buf.size();
                resp.parse(&parser, buf.front(), len);
                buf.remove(len);
                if (resp.body.size() >= max_content_length) {
                    s.reset(); // close this socket, we won't read anymore
                    return resp;
                }
            }
            // should not be any data left over in buf
            CHECK(buf.size() == 0);

            return resp;
        } catch (errorx &e) {
            s.reset();
            throw;
        }
    }

    http_response get(const std::string &path) {
        return perform("GET", path);
    }

    http_response post(const std::string &path, const std::string &data) {
        return perform("POST", path, data);
    }

};

class http_pool : public shared_pool<http_client> {
public:
    http_pool(const std::string &host_, uint16_t port_, ssize_t max_conn)
        : shared_pool<http_client>("http://" + host_,
            std::bind(&http_pool::new_resource, this),
            max_conn
        ),
        host(host_), port(port_) {}

protected:
    std::string host;
    uint16_t port;

    std::shared_ptr<http_client> new_resource() {
        VLOG(3) << "new http_client resource " << host << ":" << port;
        return std::make_shared<http_client>(host, port);
    }
};

} // end namespace ten
#endif // HTTP_CLIENT_HH

