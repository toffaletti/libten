#ifndef LIBTEN_HTTP_CLIENT_HH
#define LIBTEN_HTTP_CLIENT_HH

#include "ten/http/http_message.hh"
#include "ten/shared_pool.hh"
#include "ten/buffer.hh"
#include "ten/net.hh"
#include "ten/uri.hh"

namespace ten {

//! thrown on http network errors
class http_error : public errorx {
public:
    http_error(const std::string &msg) : errorx(msg) {}
};

//! basic http client
class http_client {
private:
    std::shared_ptr<netsock> _s;
    buffer _buf;
    std::string _host;
    uint16_t _port;

    void ensure_connection() {
        if (_s && _s->valid()) return;
        _s.reset(new netsock(AF_INET, SOCK_STREAM));
        if (_s->dial(_host.c_str(), _port) != 0) {
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

    http_response perform(const std::string &method, const std::string &path, const std::string &data="") {
        uri u;
        u.scheme = "http";
        u.host = _host;
        u.port = _port;
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
            ssize_t nw = _s->send(hdata.c_str(), hdata.size());
            if (r.body.size()) {
                nw = _s->send(r.body.c_str(), r.body.size());
            }

            http_parser parser;
            http_response resp;
            resp.parser_init(&parser);

            _buf.clear();

            while (!resp.complete) {
                _buf.reserve(4*1024);
                ssize_t nr = _s->recv(_buf.back(), _buf.available());
                if (nr <= 0) { throw http_error("recv"); }
                _buf.commit(nr);
                size_t len = _buf.size();
                resp.parse(&parser, _buf.front(), len);
                _buf.remove(len);
                if (resp.body.size() >= max_content_length) {
                    _s.reset(); // close this socket, we won't read anymore
                    return resp;
                }
            }
            // should not be any data left over in _buf
            CHECK(_buf.size() == 0);

            if ((resp.http_version == "HTTP/1.0" && resp.get("Connection") != "Keep-Alive") ||
                    resp.get("Connection") == "close")
            {
                _s.reset();
            }
            return resp;
        } catch (errorx &e) {
            _s.reset();
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
