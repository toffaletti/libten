#ifndef LIBTEN_HTTP_MESSAGE_HH
#define LIBTEN_HTTP_MESSAGE_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <stdarg.h>
#include <boost/lexical_cast.hpp>

#include "http_parser.h"
#include "ten/error.hh"

namespace ten {

// TODO: define exceptions

typedef std::pair<std::string, std::string> header_pair;
typedef std::vector<header_pair> header_list;

const size_t HEADER_RESERVE = 5;

//! http headers
struct Headers {
    header_list headers;

    Headers() {}

    template <typename ...Args>
    Headers(Args&& ...args) {
        static_assert((sizeof...(args) % 2) == 0, "mismatched header name/value pairs");
        headers.reserve(sizeof...(args) / 2);
        init(std::forward<Args>(args)...);
    }

    void init() {}
    template <typename ValueT, typename ...Args>
    void init(std::string &&header_name, ValueT &&header_value, Args&& ...args) {
        append<ValueT>(std::move(header_name), std::move(header_value));
        init(std::forward<Args>(args)...);
    }

    void set(const std::string &field, const std::string &value);

    template <typename ValueT>
        void set(const std::string &field, const ValueT &value) {
            set(field, boost::lexical_cast<std::string>(value));
        }

    void append(const std::string &field, const std::string &value);

    template <typename ValueT>
        void append(const std::string &field, const ValueT &value) {
            append(field, boost::lexical_cast<std::string>(value));
        }

    header_list::iterator find(const std::string &field);

    bool remove(const std::string &field);

    std::string get(const std::string &field) const;

    template <typename ValueT>
        ValueT get(const std::string &field) const {
            std::string val = get(field);
            if (val.empty()) {
                return ValueT();
            }
            return boost::lexical_cast<ValueT>(val);
        }
};

//! base class for http request and response
struct http_base : Headers {
    bool complete;
    std::string body;
    size_t body_length;

    explicit http_base(Headers headers_ = Headers()) :
        Headers(std::move(headers_)), complete(false), body_length(0) {}

    void set_body(const std::string &body_,
            const std::string &content_type="")
    {
        body = body_;
        body_length = body.size();
        set("Content-Length", body_length);
        remove("Content-Type");
        if (!content_type.empty()) {
            append("Content-Type", content_type);
        }
    }
};

//! http request
struct http_request : http_base {
    std::string method;
    std::string uri;
    std::string http_version;

    http_request() : http_base() {}
    http_request(std::string method_,
        std::string uri_,
        Headers headers_ = Headers(),
        std::string http_version_ = "HTTP/1.1")
        : http_base(std::move(headers_)),
        method(std::move(method_)), uri(std::move(uri_)), http_version(std::move(http_version_)) {}

    void clear() {
        headers.clear();
        complete = false;
        method.clear();
        uri.clear();
        http_version.clear();
        body.clear();
        body_length = 0;
    }

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;

    std::string path() const {
        std::string p = uri;
        size_t pos = p.find_first_of("?#");
        if (pos != std::string::npos) {
            p = p.substr(0, pos);
        }
        return p;
    }
};

//! http response
struct http_response : http_base {
    std::string http_version;
    unsigned long status_code;
    http_request *req;

    http_response(http_request *req_) : http_base(), req(req_) {}

    http_response(unsigned long status_code_ = 200,
        Headers headers_ = Headers(),
        std::string http_version_ = "HTTP/1.1")
        : http_base(std::move(headers_)),
        http_version(std::move(http_version_)),
        status_code(std::move(status_code_)),
        req(NULL)
    {
    }

    void clear() {
        headers.clear();
        http_version.clear();
        status_code = 0;
        body.clear();
        body_length = 0;
    }

    const std::string &reason() const;

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;
};

} // end namespace ten

#endif // LIBTEN_HTTP_MESSAGE_HH
