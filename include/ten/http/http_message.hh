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

using std::pair;
using std::vector;
using std::string;
using std::move;
using boost::lexical_cast;

// TODO: define exceptions

typedef pair<string, string> header_pair;
typedef vector<header_pair> header_list;

const size_t HEADER_RESERVE = 5;

//! http headers
struct Headers {
    header_list headers;

    Headers() {}

    template <typename ValueT, typename ...Args>
    Headers(string header_name, ValueT header_value, Args ...args) {
        headers.reserve(std::max(HEADER_RESERVE, sizeof...(args)));
        init(move(header_name), move(header_value), move(args)...);
    }

    // init can go away with delegating constructor support
    void init() {}
    template <typename ValueT, typename ...Args>
    void init(string header_name, ValueT header_value, Args ...args) {
        append<ValueT>(move(header_name), move(header_value));
        init(move(args)...);
    }

    void set(const string &field, const string &value);

    template <typename ValueT>
        void set(const string &field, const ValueT &value) {
            set(field, lexical_cast<string>(value));
        }

    void append(const string &field, const string &value);

    template <typename ValueT>
        void append(const string &field, const ValueT &value) {
            append(field, lexical_cast<string>(value));
        }

    header_list::iterator find(const string &field);

    bool remove(const string &field);

    string get(const string &field) const;

    template <typename ValueT>
        ValueT get(const string &field) const {
            string val = get(field);
            if (val.empty()) {
                return ValueT();
            }
            return lexical_cast<ValueT>(val);
        }
};

//! base class for http request and response
struct http_base : Headers {
    bool complete;
    string body;
    size_t body_length;

    explicit http_base(Headers headers_ = Headers()) :
        Headers(move(headers_)), complete(false), body_length(0) {}

    void set_body(const string &body_,
            const string &content_type="")
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
    string method;
    string uri;
    string http_version;

    http_request() : http_base() {}
    http_request(string method_,
        string uri_,
        Headers headers_ = Headers(),
        string http_version_ = "HTTP/1.1")
        : http_base(move(headers_)),
        method(move(method_)), uri(move(uri_)), http_version(move(http_version_)) {}

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

    string data() const;

    string path() const {
        string p = uri;
        size_t pos = p.find_first_of("?#");
        if (pos != string::npos) {
            p = p.substr(0, pos);
        }
        return p;
    }
};

//! http response
struct http_response : http_base {
    string http_version;
    unsigned long status_code;
    http_request *req;

    http_response(http_request *req_) : http_base(), req(req_) {}

    http_response(unsigned long status_code_ = 200,
        Headers headers_ = Headers(),
        string http_version_ = "HTTP/1.1")
        : http_base(move(headers_)),
        http_version(move(http_version_)),
        status_code(move(status_code_)),
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

    const string &reason() const;

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    string data() const;
};

} // end namespace ten

#endif // LIBTEN_HTTP_MESSAGE_HH
