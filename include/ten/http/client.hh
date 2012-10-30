#ifndef LIBTEN_HTTP_CLIENT_HH
#define LIBTEN_HTTP_CLIENT_HH

#include "ten/http/http_message.hh"
#include "ten/shared_pool.hh"
#include "ten/buffer.hh"
#include "ten/net.hh"
#include "ten/uri.hh"
#include <boost/algorithm/string/compare.hpp>

namespace ten {

//! thrown on http network errors
class http_error : public errorx {
public:
    http_error(const std::string &msg) : errorx(msg) {}
};

//! basic http client
class http_client {
private:
    netsock _sock;
    buffer _buf;
    std::string _host;
    uint16_t _port;

    void ensure_connection() {
        if (_sock.valid()) return;
        _sock = std::move(netsock(AF_INET, SOCK_STREAM));
        if (_sock.dial(_host.c_str(), _port) != 0) {
            throw http_error("dial");
        }
    }

public:
    size_t max_content_length;

    http_client(const std::string &host_, uint16_t port_=80)
        : _buf(4*1024), _host(host_), _port(port_), max_content_length(~(size_t)0)
    {
        parse_host_port(_host, _port);
    }

    std::string host() const { return _host; }
    uint16_t port() const { return _port; }

    http_response perform(const std::string &method, const std::string &path,
                          const http_headers &hdrs = {}, const std::string &data = {},
                          std::chrono::milliseconds timeout = {})
    {
        uri u;
        u.scheme = "http";
        u.host = _host;
        u.port = _port;
        u.path = path;
        u.normalize();

        http_request r(method, u.compose_path(), hdrs);
        // HTTP/1.1 requires host header
        if (r.find("Host") == r.headers.end())
            r.set("Host", u.host); 
        r.body = data;

        return perform(r, timeout);
    }

    http_response perform(http_request &r, std::chrono::milliseconds timeout = {}) {
        if (r.body.size()) {
            r.set("Content-Length", r.body.size());
        }

        // should really set a time in the future and calculate this each time
        unsigned ms = boost::numeric_cast<unsigned>(timeout.count());

        try {
            ensure_connection();

            http_response resp(&r);

            std::string hdata = r.data();
            ssize_t nw = _sock.send(hdata.c_str(), hdata.size(), 0, ms);
            bool badw = (size_t)nw != hdata.size();
            if (!badw && r.body.size()) {
                nw = _sock.send(r.body.c_str(), r.body.size(), 0, ms);
                badw = (size_t)nw != r.body.size();
            }
            if (badw) {
                _sock.close(); // socket is unusable after write failure
                resp.status_code = 499;
                resp.set_body("Short write");
                return resp;
            }

            http_parser parser;
            resp.parser_init(&parser);

            _buf.clear();

            while (!resp.complete) {
                _buf.reserve(4*1024);
                ssize_t nr = _sock.recv(_buf.back(), _buf.available(), 0, ms);
                if (nr <= 0) { throw http_error("recv"); }
                _buf.commit(nr);
                size_t len = _buf.size();
                resp.parse(&parser, _buf.front(), len);
                _buf.remove(len);
                if (resp.body.size() >= max_content_length) {
                    _sock.close(); // close this socket, we won't read anymore
                    return resp;
                }
            }
            // should not be any data left over in _buf
            CHECK(_buf.size() == 0);

            const auto conn = resp.get("Connection");
            if (boost::iequals(conn, "close")
                || (resp.http_version == http_1_0 && !boost::iequals(conn, "Keep-Alive")))
            {
                _sock.close();
            }
            return resp;
        } catch (errorx &e) {
            _sock.close();
            throw;
        }
    }

    http_response get(const std::string &path, std::chrono::milliseconds timeout = {}) {
        return perform("GET", path, {}, {}, timeout);
    }

    http_response post(const std::string &path, const std::string &data, std::chrono::milliseconds timeout = {}) {
        return perform("POST", path, {}, data, timeout);
    }

};

class http_pool : public shared_pool<http_client> {
public:
    http_pool(const std::string &host_, uint16_t port_, ssize_t max_conn)
        : shared_pool<http_client>("http://" + host_ + ":" + std::to_string(port_),
            std::bind(&http_pool::new_resource, this),
            max_conn
        ),
        _host(host_), _port(port_) {}

protected:
    std::string _host;
    uint16_t _port;

    std::shared_ptr<http_client> new_resource() {
        VLOG(3) << "new http_client resource " << _host << ":" << _port;
        return std::make_shared<http_client>(_host, _port);
    }
};

} // end namespace ten

#endif // LIBTEN_HTTP_CLIENT_HH
