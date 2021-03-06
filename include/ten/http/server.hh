#ifndef LIBTEN_HTTP_SERVER_HH
#define LIBTEN_HTTP_SERVER_HH

#include <fnmatch.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <chrono>
#include <functional>
#include <netinet/tcp.h>

#include "ten/buffer.hh"
#include "ten/logging.hh"
#include "ten/net.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"

namespace ten {

//! http request/response pair; keeps reference to request and socket
// (the term "exchange" appears in the standard)

struct http_exchange {
    typedef std::function<void (http_exchange &)> log_func_t;

    http_request &req;
    netsock &sock;
    http_response resp{ HTTP_Not_Found };
    bool resp_sent{false};
    std::chrono::steady_clock::time_point start;
    log_func_t log_func;

    http_exchange(http_request &req_, netsock &sock_, const log_func_t &log_func_)
        : req(req_),
          sock(sock_),
          start(std::chrono::steady_clock::now()),
          log_func(log_func_)
        {}

    http_exchange(const http_exchange &) = delete;
    http_exchange &operator=(const http_exchange &) = delete;

    ~http_exchange() {
        if (log_func) {
            log_func(*this);
        }
        send_response(); // ensure a response is sent
        if (resp.close_after()) {
            sock.close();
        }
    }

    //! compose a uri from the request uri
    uri get_uri(optional<std::string> host = nullopt) const {
        if (!host) {
            if (boost::starts_with(req.uri, "http://")) {
                // TODO: transform to preserve passed in host
                return req.uri;
            }
            host = req.get(hs::Host);
            if (!host)
                host.emplace("localhost");
        }
        uri tmp;
        tmp.scheme = "http";
        tmp.host = *host;
        tmp.path = req.uri;
        return tmp.compose();
    }

    //! send response to this request
    ssize_t send_response() {
        if (resp_sent) return 0;
        resp_sent = true;
        // TODO: Content-Length might be good to add to normal responses,
        //    but only if Transfer-Encoding isn't chunked?
        if (resp.status_code >= 400 && resp.status_code <= 599
            && !resp.get(hs::Content_Length)
            && req.method != hs::HEAD)
        {
            resp.set(hs::Content_Length, resp.body.size());
        }

        // HTTP/1.1 requires Date, so lets add it
        if (!resp.get(hs::Date)) {
            resp.set(hs::Date, http_base::rfc822_date());
        }

        // obey client's wishes on closing if we have none of our own,
        //  else prefer to keep http 1.1 open
        if (!resp.get(hs::Connection)) {
            const auto conn_hdr = req.get(hs::Connection);
            if (conn_hdr)
                resp.set(hs::Connection, *conn_hdr);
            else if (req.version == http_1_0)
                resp.set(hs::Connection, hs::close);
        }

        auto data = resp.data();
        if (!resp.body.empty() && req.method != hs::HEAD) {
            data += resp.body;
        }
        ssize_t nw = sock.send(data.data(), data.size());
        return nw;
    }

    //! the ip of the host making the request
    //! might use the X-Forwarded-For header
    optional<std::string> agent_ip(bool use_xff=false) const {
        if (use_xff) {
            auto xff_hdr = req.get("X-Forwarded-For");
            if (xff_hdr && !(*xff_hdr).empty()) {
                const char *xff = xff_hdr->c_str();
                // pick the first addr    
                int i;
                for (i=0; *xff && i<256 && !isdigit((unsigned char)*xff); i++, xff++) {}
                if (*xff && i < 256) {
                    // now, find the end 
                    const char *e = xff;
                    for (i = 0; *e && i<256 && (isdigit((unsigned char)*e) || *e == '.'); i++, e++) {}
                    if (i < 256 && e >= xff + 7 ) {
                        return std::string(xff, e - xff);
                    }
                }
            }
        }
        address addr;
        if (sock.getpeername(addr)) {
            char buf[INET6_ADDRSTRLEN];
            if (addr.ntop(buf, sizeof(buf))) {
                return optional<std::string>(in_place, buf);
            }
        }
        return nullopt;
    }
};


//! basic http server
class http_server : public netsock_server {
public:
    using log_func_t = http_exchange::log_func_t;
    using callback_type = std::function<void (http_exchange &)>;

    std::function<void ()> connect_watch;
    std::function<void ()> disconnect_watch;

private:
    struct route {
        optional<std::string> method;
        std::string pattern;
        int fnmatch_flags;
        callback_type callback;

        route(optional<std::string> method_,
              std::string pattern_,
              const callback_type &callback_,
              int fnmatch_flags_=0)
            : method(std::move(method_)), pattern(std::move(pattern_)), fnmatch_flags(fnmatch_flags_), callback(callback_) {}

        route(const char *method_,
              std::string pattern_,
              const callback_type &callback_,
              int fnmatch_flags_=0)
            : route(optional<std::string>(method_), std::move(pattern_), callback_, fnmatch_flags_) {}

        route(std::string pattern_,
              const callback_type &callback_,
              int fnmatch_flags_=0)
            : route(nullopt, std::move(pattern_), callback_, fnmatch_flags_) {}
    };

    std::vector<route> _routes;
    log_func_t _log_func;

public:
    http_server(nostacksize_t=nostacksize, optional_timeout recv_timeout_ms_=nullopt)
        : netsock_server("http", nostacksize, recv_timeout_ms_)
    {
    }

    //! add a callback for a uri with an fnmatch pattern
    template <typename... Args>
    void add_route(Args&&... args) {
        _routes.emplace_back(std::forward<Args>(args)...);
    }

    //! set logging function, called after every exchange
    void set_log_callback(const log_func_t &f) {
        _log_func = f;
    }

private:

    void setup_listen_socket(netsock &s) override {
        netsock_server::setup_listen_socket(s);
        s.setsockopt(IPPROTO_TCP, TCP_DEFER_ACCEPT, 30);
    }

    void on_connection(netsock &s) override {
        // TODO: tuneable buffer sizes
        buffer buf(4*1024);
        http_parser parser;

        if (connect_watch) {
            connect_watch();
        }

        bool nodelay_set = false;
        http_request req;
        while (s.valid()) {
            req.parser_init(&parser);
            bool got_headers = false;
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = -1;
                if (buf.size() == 0) {
                    nr = s.recv(buf.back(), buf.available(), 0, _recv_timeout_ms);
                    if (nr <= 0) goto done;
                    buf.commit(nr);
                }
                size_t nparse = buf.size();
                req.parse(&parser, buf.front(), nparse);
                buf.remove(nparse);
                if (req.complete) {
                    DVLOG(4) << req.data();
                    // handle http exchange (request -> response)
                    http_exchange ex(req, s, _log_func);
                    if (!nodelay_set && !req.close_after()) {
                        // this is likely a persistent connection, so low-latency sending is worth the overh
                        s.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);
                        nodelay_set = true;
                    }
                    handle_exchange(ex);
                    break;
                }
                if (nr == 0) goto done;
                if (!got_headers && !req.method.empty()) {
                    got_headers = true;
                    auto exp_hdr = req.get("Expect");
                    if (exp_hdr && *exp_hdr == "100-continue") {
                        static const http_response cont_resp{ HTTP_Continue };
                        static const auto data = cont_resp.data();
                        ssize_t nw = s.send(data.data(), data.size());
                        (void)nw;
                    }
                }
            }
        }
done:
        if (disconnect_watch) {
            disconnect_watch();
        }
    }

    void handle_exchange(http_exchange &ex) const {
        const auto path = ex.req.path();
        DVLOG(5) << "path: " << path;
        // not super efficient, but good enough
        // note: empty string pattern matches everything
        bool bad_method = false;
        for (const auto &r : _routes) {
            DVLOG(5) << "matching pattern: " << r.pattern << (r.method ? " (" + *r.method + ")" : "");
            if (r.pattern.empty() || fnmatch(r.pattern.c_str(), path.c_str(), r.fnmatch_flags) == 0) {
                if (r.method && *r.method != ex.req.method) {
                    bad_method = true;
                    continue;
                }
                try {
                    r.callback(std::ref(ex));
                } catch (std::exception &e) {
                    DVLOG(2) << "unhandled exception in " << ex.req.method << " of route [" << r.pattern << "]: " << e.what();
                    ex.resp = { HTTP_Internal_Server_Error, { hs::Connection, hs::close } };
                    std::string msg = e.what();
                    if (!msg.empty() && *msg.rbegin() != '\n')
                        msg += '\n';
                    ex.resp.set_body(msg, hs::text_plain);
                }
                ex.send_response();
                break;
            }
        }
        // if at least one pattern would have matched except method was wrong, avoid nondescript 404
        if (bad_method)
            ex.resp = { HTTP_Method_Not_Allowed };
    }
};

} // end namespace ten

#endif // LIBTEN_HTTP_SERVER_HH
