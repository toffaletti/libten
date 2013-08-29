#ifndef LIBTEN_HTTP_CLIENT_HH
#define LIBTEN_HTTP_CLIENT_HH

#include "ten/http/http_message.hh"
#include "ten/http/http_error.hh"
#include "ten/shared_pool.hh"
#include "ten/buffer.hh"
#include "ten/net.hh"
#include "ten/uri.hh"
#include "ten/task.hh"
#include <atomic>
#include <chrono_io>
#include <netinet/tcp.h>

namespace ten {

//! possibly recycle connections after a while
//  TODO: move down to common base with other clients

//! basic http client
class http_client {
public:
    using lifetime_t = optional<std::chrono::seconds>;
    using retire_t = optional<kernel::time_point>;
    static constexpr double pretire_factor = 0.8;
private:
    netsock _sock;
    buffer _buf;
    std::string _host;
    uint16_t _port;
    optional_timeout _conn_timeout;
    retire_t _pretire, _retire;

    void ensure_connection() {
        if (!_sock.valid()) {
            netsock cs{AF_INET, SOCK_STREAM};
            if (!cs.valid()) {
                throw http_makesock_error{};
            }
            try {
                cs.dial(_host.c_str(), _port, _conn_timeout);
            } catch (const errno_error &e) {
                throw http_dial_error{e};
            } catch (const std::exception &e) {
                throw http_dial_error{e.what()};
            }
            cs.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
            _sock = std::move(cs);
        }
    }

public:
    size_t max_content_length = ~(size_t)0;

    http_client(const std::string &host_, uint16_t port_ = 80,
                lifetime_t lifetime = nullopt, optional_timeout conn_timeout = nullopt)
        : _buf(4*1024), _host(host_), _port(port_), _conn_timeout(conn_timeout)
    {
        parse_host_port(_host, _port);

        if (lifetime) {
            _retire = kernel::now() + *lifetime;
            const auto pre_elig = (intmax_t)(lifetime->count() * pretire_factor);
            if (pre_elig >= 1)
                _pretire = kernel::now() + lifetime_t::value_type(pre_elig);
        }
    }

    std::string host() const                       { return _host; }
    uint16_t port() const                          { return _port; }
    optional_timeout conn_timeout() const          { return _conn_timeout; }
    retire_t pretire() const                       { return _pretire; }
    retire_t retire() const                        { return _retire; }
    bool pretire_by(kernel::time_point when) const { return _pretire && when >= *_pretire; }
    bool retire_by(kernel::time_point when) const  { return _retire  && when >= *_retire; }

    http_response get(const std::string &path, optional_timeout timeout = nullopt) {
        return perform(hs::GET, path, {}, {}, timeout);
    }
    http_response delete_(const std::string &path, optional_timeout timeout = nullopt) {
        return perform(hs::DELETE, path, {}, {}, timeout);
    }
    http_response post(const std::string &path, std::string data, optional_timeout timeout = nullopt) {
        return perform(hs::POST, path, {}, std::move(data), timeout);
    }
    http_response put(const std::string &path, std::string data, optional_timeout timeout = nullopt) {
        return perform(hs::PUT, path, {}, std::move(data), timeout);
    }

    http_response perform(const std::string &method, const std::string &path,
                          http_headers hdrs = {}, std::string data = {},
                          optional_timeout timeout = nullopt)
    {
        // Calculate canonical path
        uri u;
        u.scheme = "http";
        u.host = _host;
        u.port = _port;
        u.path = path;
        u.normalize();

        // HTTP/1.1 requires host header; include it always, why not
        if (!hdrs.contains(hs::Host))
            hdrs.set(hs::Host, u.host);

        http_request r{method, u.compose_path(), std::move(hdrs), std::move(data)};
        return perform(r, timeout);
    }

    http_response perform(http_request &r, optional_timeout timeout = nullopt) {
        VLOG(4) << "-> " << r.method << " " << _host << ":" << _port << " " << r.uri;

        if (r.body.size()) {
            r.set(hs::Content_Length, r.body.size());
        }

        try {
            ensure_connection();

            std::string data = r.data();
            if (r.body.size() > 0) {
                data += r.body;
            }
            ssize_t nw = _sock.send(data.data(), data.size(), 0, timeout);
            if (nw < 0) {
                throw http_send_error{};
            }
            else if ((size_t)nw != data.size()) {
                std::ostringstream ss;
                ss << "short write: " << nw << " < " << data.size();
                throw http_error(ss.str().c_str());
            }

            http_response resp;
            http_parser parser;
            resp.parser_init(&parser, r.method == hs::HEAD);

            _buf.clear();

            while (!resp.complete) {
                _buf.reserve(4*1024);
                ssize_t nr = _sock.recv(_buf.back(), _buf.available(), 0, timeout);
                if (nr < 0) { throw http_recv_error{}; }
                if (!nr) { throw http_closed_error{}; }
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

            // if response requests closing socket, do it
            if (resp.close_after())
                _sock.close();

            VLOG(4) << "<- " << resp.status_code << " [" << resp.body.size() << "]";
            return resp;
        } catch (errorx &e) {
            if (_sock.valid())
                _sock.close();
            throw;
        }
    }
};

namespace detail {
class http_scoped_resource;
}

class http_pool : public shared_pool<http_client> {
    friend class detail::http_scoped_resource;

public:
    using scoped_resource = detail::http_scoped_resource;
    using host_port_t = std::pair<std::string, uint16_t>;
    using host_port_factory_t = std::function< host_port_t() >;

    // general case: arbitrary host generator
    http_pool(std::string name_,
              host_port_factory_t host_factory_,
              optional<size_t> max_conn_ = nullopt,
              http_client::lifetime_t lifetime_ = nullopt,
              optional_timeout conn_timeout_ = nullopt)
        : shared_pool<http_client>(
            name_,
            [this]{ return new_resource(); },
            max_conn_
          ),
          _host_factory(host_factory_),
          _lifetime(lifetime_),
          _conn_timeout(conn_timeout_)
    {
        if (_lifetime) {
            if (_lifetime->count() <= 0)
                throw_stream() << name() << ": invalid client lifetime " << *_lifetime;
            _lt = std::make_shared<lt_state>();
            _lt->t = task::spawn([=] {
                _lt_run(_impl(), _lt, _lifetime);
            });
        }
    }

    // simple case: one host
    http_pool(std::string host_,
              uint16_t port_,
              optional<size_t> max_conn_ = nullopt,
              http_client::lifetime_t lifetime_ = nullopt,
              optional_timeout conn_timeout_ = nullopt)
        : http_pool(
            "http://" + host_ + ":" + std::to_string(port_),
            [=]{ return host_port_t{ host_, port_ }; },
            max_conn_,
            lifetime_,
            conn_timeout_
          )
    {}

protected:
    host_port_factory_t _host_factory;
    http_client::lifetime_t _lifetime;
    optional_timeout _conn_timeout;

    struct lt_state {
        task t;
        std::atomic<size_t> retired{};
        rendez worker;

        ~lt_state() {
            t.cancel();
        }
    };
    std::shared_ptr<lt_state> _lt;

    res_ptr new_resource() {
        const auto hp = _host_factory();
        VLOG(3) << name() << ": new http_client resource " << hp.first << ":" << hp.second;
        return std::make_shared<http_client>(hp.first, hp.second, _lifetime, _conn_timeout);
    }

    // periodically check the items at the front of the avail queue.
    // if they're 2x the max age, then they're excess and stale; toss 'em.
    // (but defer actual destruction until the pool_impl is unlocked.)
    // somewhat less stale connections are handled by the custom scoped_resource.
    static void _lt_run(std::shared_ptr<pool_impl> im,
                        std::shared_ptr<lt_state> lt,
                        http_client::lifetime_t lifetime)
    {
        const std::chrono::milliseconds interval{500 * std::min((std::chrono::milliseconds::rep)20L, lifetime->count())};
        VLOG(3) << im->name << ": LT interval=" << interval.count();
        for (;;) {
            std::vector<res_ptr> doomed; // declared first, destroyed last
            std::unique_lock<qutex> lk(im->mut);

            VLOG(4) << im->name << ": LT asleep";
            try {
                deadline dl{interval};
                lt->worker.sleep(lk);
            }
            catch (deadline_reached &) {
                // that's ok.  time to check the donuts
            }
            VLOG(4) << im->name << ": LT awake"
                    << "; retired=" << lt->retired.load()
                    << ", set=" << im->set.size()
                    << ", avail=" << im->avail.size();

            // perform requested replacements

            for (; lt->retired.load(); lt->retired--) {
                res_ptr c;
                try {
                    c = im->new_with_lock(lk);
                }
                catch (task_interrupted &) {
                    // let the task die
                    throw;
                }
                catch (std::exception &) {
                    // any other exception has been logged; skip this replacement
                    continue;
                }
                VLOG(4) << "LT: created " << c;
                im->avail.push_front(c);
                im->not_empty.wakeup();
            }

            // move deeply stale items into destructobucket, which then... destructs

            while (!im->avail.empty()) {
                auto c = im->avail.back();
                if (!c->retire_by(kernel::now()))
                    break;
                VLOG(3) << im->name << ": LT dropping stale " << c;
                im->avail.pop_back();
                im->set.erase(c);
                doomed.push_back(std::move(c));
            }
            if (!doomed.empty())
                im->not_empty.wakeup();  // in theory, this should never matter
        }
    }
};

namespace detail {

class http_scoped_resource : public scoped_resource<http_client> {
    std::shared_ptr<http_pool::lt_state> _lt;
    bool _mustdie;

public:
    //! RAII
    explicit http_scoped_resource(http_pool &p)
        : scoped_resource{p},
          _lt{p._lt},
          _mustdie{_c->pretire_by(kernel::now())}
    {
        if (_mustdie) {
            _lt->retired++;
            VLOG(4) << "LT: waking task, retired=" << _lt->retired.load();
            _lt->worker.wakeup();
        }
    }

    ~http_scoped_resource() {
        if (_mustdie)
            _success = false;
    }
};

} // end namespace detail

} // end namespace ten

#endif // LIBTEN_HTTP_CLIENT_HH
