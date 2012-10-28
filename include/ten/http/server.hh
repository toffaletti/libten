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
#include "ten/task.hh"
#include "ten/net.hh"
#include "ten/http/http_message.hh"
#include "ten/uri.hh"

namespace ten {

//! basic http server
class http_server : public netsock_server {
public:

    //! wrapper around an http request/response
    struct request {
        http_request &req;
        netsock &sock;
        http_response resp {404};
        bool resp_sent {false};
        std::chrono::high_resolution_clock::time_point start;

        request(http_request &req_, netsock &sock_)
            : req(req_), sock(sock_),
              start(std::chrono::steady_clock::now()) {}

        //! compose a uri from the request uri
        uri get_uri(std::string host="") {
            if (host.empty()) {
                host = req.get("Host");
                // TODO: transform to preserve passed in host
                if (boost::starts_with(req.uri, "http://")) {
                    return req.uri;
                }
            }

            if (host.empty()) {
                // just make up a host
                host = "localhost";
            }
            uri tmp;
            tmp.host = host;
            tmp.scheme = "http";
            tmp.path = req.uri;
            return tmp.compose();
        }

        //! send response to this request
        ssize_t send_response() {
            if (resp_sent) return 0;
            resp_sent = true;
            std::string data;
            // TODO: Content-Length might be another good one to add here
            // but only if Transfer-Encoding isn't chunked?
            // HTTP/1.1 requires Date, so lets add it
            if (resp.get("Date").empty()) {
                char buf[128];
                struct tm tm;
                time_t now = std::chrono::system_clock::to_time_t(procnow());
                strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&now, &tm));
                resp.set("Date", buf);
            }
            std::string conn = req.get("Connection");
            if (!conn.empty() && resp.get("Connection").empty()) {
                // obey clients wishes if we have none of our own
                resp.set("Connection", conn);
            }
            if (req.http_version == "HTTP/1.0" && req.get("Connection").empty()) {
                resp.set("Connection", "close");
            }

            data = resp.data();
            if (!resp.body.empty() && req.method != "HEAD") {
                data += resp.body;
            }
            ssize_t nw = sock.send(data.data(), data.size());
            return nw;
        }

        //! the ip of the host making the request
        //! might use the X-Forwarded-For header
        std::string agent_ip(bool use_xff=false) const {
            if (use_xff) {
                std::string xffs = req.get("X-Forwarded-For");
                const char *xff = xffs.c_str();
                if (xff) {
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
                    return buf;
                }
            }
            return "";
        }

        ~request() {
            if (!resp_sent) {
                // ensure a response is sent
                resp = http_response(404, http_headers{"Content-Length", 0});
                send_response();
            }

            if (boost::iequals(resp.get("Connection"), "close")) {
                sock.close();
            }
        }
    };

    typedef std::function<void (request &)> callback_type;
public:
    std::function<void ()> connect_watch;
    std::function<void ()> disconnect_watch;
protected:
    struct route {
        std::string pattern;
        callback_type callback;
        int fnmatch_flags;

        route(const std::string &pattern_,
                const callback_type &callback_,
                int fnmatch_flags_=0)
            : pattern(pattern_), callback(callback_), fnmatch_flags(fnmatch_flags_) {}
    };

    std::vector<route> _routes;
    callback_type _log_func;
public:
    http_server(size_t stacksize_=default_stacksize, unsigned timeout_ms_=0)
        : netsock_server("http", stacksize_, timeout_ms_)
    {
    }

    //! add a callback for a uri with an fnmatch pattern
    void add_route(const std::string &pattern,
            const callback_type &callback,
            int fnmatch_flags = 0)
    {
        _routes.emplace_back(pattern, callback, fnmatch_flags);
    }

    //! set logging function, called after every request
    void set_log_callback(const callback_type &f) {
        _log_func = f;
    }

    void on_connection(netsock &s) {
        // TODO: tuneable buffer sizes
        buffer buf(4*1024);
        http_parser parser;

        if (connect_watch) {
            connect_watch();
        }
        // TODO: might want to enable this later after
        // we know this will be a long-lived connection
        // otherwise the overhead of the syscall hurts concurrency
        //s.s.setsockopt(IPPROTO_TCP, TCP_NODELAY, 1);

        http_request req;
        for (;;) {
            req.parser_init(&parser);
            bool got_headers = false;
            for (;;) {
                buf.reserve(4*1024);
                ssize_t nr = -1;
                if (buf.size() == 0) {
                    nr = s.recv(buf.back(), buf.available(), _timeout_ms);
                    if (nr < 0) goto done;
                    buf.commit(nr);
                }
                size_t nparse = buf.size();
                req.parse(&parser, buf.front(), nparse);
                buf.remove(nparse);
                if (req.complete) {
                    // handle request
                    handle_request(req, s);
                    break;
                }
                if (nr == 0) goto done;
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
        }
done:
        if (disconnect_watch) {
            disconnect_watch();
        }
    }

    void handle_request(http_request &req, netsock &s) {
        request r(req, s);
        const auto path = req.path();
        DVLOG(5) << "path: " << path;
        // not super efficient, but good enough
        for (const auto &i : _routes) {
            DVLOG(5) << "matching pattern: " << i.pattern;
            if (fnmatch(i.pattern.c_str(), path.c_str(), i.fnmatch_flags) == 0) {
                try {
                    i.callback(r);
                } catch (std::exception &e) {
                    DVLOG(2) << "unhandled exception in route [" << i.pattern << "]: " << e.what();
                    r.resp = http_response(500, http_headers{"Connection", "close"});
                    std::string msg = e.what();
                    if (!msg.empty() && *msg.rbegin() != '\n')
                        msg += '\n';
                    r.resp.set_body(msg);
                    r.send_response();
                }
                break;
            }
        }
        if (_log_func) {
            _log_func(r);
        }
    }
};

} // end namespace ten

#endif // LIBTEN_HTTP_SERVER_HH
