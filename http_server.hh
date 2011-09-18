#include <fnmatch.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/bind.hpp>

#include "logging.hh"
#include "task.hh"
#include "http/http_message.hh"
#include "uri/uri.hh"

namespace fw {

//! simple http server
class http_server : boost::noncopyable {
public:
    //! wrapper around an http request/response
    struct request {
        request(http_request &req_, task::socket &sock_)
            : req(req_), resp(404, "Not Found"), sock(sock_),
            start(boost::posix_time::microsec_clock::universal_time()),
            resp_sent(false) {}

        //! compose a uri from the request uri
        uri get_uri(std::string host="") {
            if (host.empty()) {
                host = req.header_string("Host");
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
            tmp.path = req.uri.c_str();
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
            if (resp.header_string("Date").empty()) {
                char buf[128];
                struct tm tm;
                time_t now = time(NULL);
                strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&now, &tm));
                resp.set_header("Date", buf);
            }
            data = resp.data();
            ssize_t nw = sock.send(data.data(), data.size());
            // TODO: send body?
            return nw;
        }

        //! the ip of the host making the request
        //! might use the X-Forwarded-For header
        std::string agent_ip(bool use_xff=false) const {
            if (use_xff) {
                std::string xffs = req.header_string("X-Forwarded-For");
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
                return addr.ntop(buf, sizeof(buf));
            }
            return "";
        }

        ~request() {
            if (!resp_sent) {
                // ensure a response is sent
                resp = http_response(404, "Not Found");
                resp.append_header("Connection", "close");
                resp.append_header("Content-Length", 0);
                send_response();
            }

            if (resp.header_string("Connection") == "close") {
                sock.close();
            }
        }

        http_request &req;
        http_response resp;
        task::socket &sock;
        boost::posix_time::ptime start;
        bool resp_sent;
    };

    typedef boost::function<void (request &)> func_type;
    typedef boost::tuple<std::string, func_type> tuple_type;
    typedef std::vector<tuple_type> map_type;

public:
    http_server(size_t stack_size_=DEFAULT_STACK_SIZE, int timeout_ms_=-1)
        : sock(AF_INET, SOCK_STREAM), stack_size(stack_size_), timeout_ms(timeout_ms_)
    {
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    }

    //! add a callback for a uri with an fnmatch pattern
    void add_callback(const std::string &pattern, const func_type &f) {
      _map.push_back(tuple_type(pattern, f));
    }

    //! set logging function, called after every request
    void set_log_callback(const func_type &f) {
        _log_func = f;
    }

    //! spawn the listening task
    void serve(const std::string &ipaddr, uint16_t port) {
        address baddr(ipaddr.c_str(), port);
        sock.bind(baddr);
        sock.getsockname(baddr);
        LOG(INFO) << "listening on: " << baddr;
        task::spawn(boost::bind(&http_server::listen_task, this));
    }

private:
    task::socket sock;
    map_type _map;
    size_t stack_size;
    int timeout_ms;
    func_type _log_func;

    void listen_task() {
        sock.listen();

        for (;;) {
            address client_addr;
            int fd;
            while ((fd = sock.accept(client_addr, 0)) > 0) {
                task::spawn(boost::bind(&http_server::client_task, this, fd), 0, stack_size);
            }
        }
    }

    void client_task(int fd) {
        task::socket s(fd);
        buffer buf(4*1024);
        http_parser parser;

        buffer::slice rb = buf(0);
        for (;;) {
            http_request req;
            req.parser_init(&parser);
            for (;;) {
                ssize_t nr = s.recv(rb.data(), rb.size(), timeout_ms);
                if (nr < 0) return;
                if (req.parse(&parser, rb.data(), nr)) break;
                if (nr == 0) return;
            }
            // handle request
            handle_request(req, s);
        }
    }

    void handle_request(http_request &req, task::socket &s) {
        request r(req, s);
        uri u = r.get_uri();
        DVLOG(5) << "path: " << u.path;
        // not super efficient, but good enough
        for (map_type::const_iterator i= _map.begin(); i!= _map.end(); i++) {
            DVLOG(5) << "matching pattern: " << i->get<0>();
            if (fnmatch(i->get<0>().c_str(), u.path.c_str(), 0) == 0) {
                i->get<1>()(r);
                break;
            }
        }
        if (_log_func) {
            _log_func(r);
        }
    }
};

} // end namespace fw

