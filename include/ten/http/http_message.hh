#ifndef LIBTEN_HTTP_MESSAGE_HH
#define LIBTEN_HTTP_MESSAGE_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <stdarg.h>
#include <boost/lexical_cast.hpp>

#include "http_parser.h"
#include "ten/error.hh"
#include "ten/maybe.hh"

namespace ten {

// TODO: define exceptions

typedef std::pair<std::string, std::string> header_pair;
typedef std::vector<header_pair> header_list;

extern const std::string http_1_0; //"HTTP/1.0"
extern const std::string http_1_1; //"HTTP/1.1"

//! http headers
struct http_headers {
    header_list headers;

    http_headers() {}

    template <typename ...Args>
        http_headers(Args&& ...args) {
            static_assert((sizeof...(args) % 2) == 0, "mismatched header name/value pairs");
            headers.reserve(sizeof...(args) / 2);
            init(std::forward<Args>(args)...);
        }

    void init() {}

    template <typename ValueT, typename ...Args>
    void init(const std::string &field, ValueT &&header_value, Args&& ...args) {
        append(field, std::forward<ValueT>(header_value));
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

    header_list::iterator       find(const std::string &field);
    header_list::const_iterator find(const std::string &field) const;

    bool remove(const std::string &field);

    std::string get(const std::string &field) const;
    maybe<std::string> mget(const std::string &field) const;

    template <typename ValueT>
        ValueT get(const std::string &field) const {
            auto i = find(field);
            return (i == headers.end()) ? ValueT() : boost::lexical_cast<ValueT>(i->second);
        }
    template <typename ValueT>
        maybe<ValueT> mget(const std::string &field) const {
            auto i = find(field);
            return (i == headers.end()) ? nothing : boost::lexical_cast<ValueT>(i->second);
        }

#ifdef CHIP_UNSURE

    bool is(const std::string &field, const std::string &value) const;
    bool is_nocase(const std::string &field, const std::string &value) const;

    template <typename ValueT>
        bool is(const std::string &field, const ValueT &value) const {
            auto i = headers.find(field);
            return (i != headers.end()) && (boost::lexical_cast<ValueT>(i->second) == value);
        }

#endif
};

//! base class for http request and response
struct http_base : http_headers {
    std::string body;
    size_t body_length {};
    bool complete {};

    explicit http_base(http_headers headers_ = http_headers())
        : http_headers(std::move(headers_)) {}

    void clear() {
        headers.clear();
        body.clear();
        body_length = 0;
        complete = false;
    }

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
                 http_headers headers_ = http_headers(),
                 std::string http_version_ = http_1_1)
        : http_base(std::move(headers_)),
          method(std::move(method_)),
          uri(std::move(uri_)),
          http_version(std::move(http_version_))
        {}

    void clear() {
        http_base::clear();
        method.clear();
        uri.clear();
        http_version.clear();
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
    bool guillotine {};  // return only the head
    std::string http_version;
    unsigned long status_code {};

    http_response(http_request *req_) : http_base(), guillotine{req_ && req_->method == "HEAD"} {}

    http_response(unsigned long status_code_ = 200,
                  http_headers headers_ = http_headers(),
                  std::string http_version_ = http_1_1)
        : http_base(std::move(headers_)),
          http_version(std::move(http_version_)),
          status_code(std::move(status_code_))
        {}

    void clear() {
        http_base::clear();
        http_version.clear();
        status_code = 0;
    }

    const std::string &reason() const;

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;
};

} // end namespace ten

#endif // LIBTEN_HTTP_MESSAGE_HH
