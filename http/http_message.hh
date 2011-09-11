#ifndef HTTP_HH
#define HTTP_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <stdarg.h>

#include "http_parser.h"
#include "error.hh"

// TODO: define exceptions

typedef std::pair<std::string, std::string> header_pair;
typedef std::vector<header_pair> header_list;

struct http_base {
    header_list headers;
    bool complete;
    std::string body;
    size_t body_length;

    http_base() : complete(false), body_length(0) {}

    void set_header(const std::string &field, const std::string &value);
    void set_header(const std::string &field, unsigned long long value);
    void append_header(const std::string &field, const std::string &value);
    void append_header(const std::string &field, unsigned long long value);
    header_list::iterator find_header(const std::string &field);
    bool remove_header(const std::string &field);
    std::string header_string(const std::string &field) const;
    unsigned long long header_ull(const std::string &field) const;

    void normalize_headers();
    void set_body(const std::string &body_) {
        body = body_;
        body_length = body.size();
        remove_header("Content-Length");
        append_header("Content-Length", body_length);
    }
};

struct http_request : http_base {
    std::string method;
    std::string uri;
    std::string http_version;

    http_request() {}
    http_request(const std::string &method_,
        const std::string &uri_,
        const std::string &http_version_ = "HTTP/1.1")
        : method(method_), uri(uri_), http_version(http_version_) {}

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
    bool parse(struct http_parser *p, const char *data, size_t len);

    std::string data() const;
};

struct http_response : http_base {
    std::string http_version;
    unsigned long status_code;
    std::string reason;
    http_request *req;

    http_response(http_request *req_) : req(req_) {}

    http_response(unsigned long status_code_ = 200,
        const std::string &reason_ = "OK",
        const std::string &http_version_ = "HTTP/1.1",
        unsigned int nheaders = 0,
        ...) // headers
        : http_version(http_version_),
        status_code(status_code_),
        reason(reason_),
        req(NULL)
    {
        va_list ap;
        va_start(ap, nheaders);
        for (;nheaders > 0;--nheaders) {
            const char *header_name = va_arg(ap, const char *);
            const char *header_value = va_arg(ap, const char *);
            append_header(header_name, header_value);
        }
        va_end(ap);
    }

    void clear() {
        headers.clear();
        http_version.clear();
        status_code = 0;
        reason.clear();
        body.clear();
        body_length = 0;
    }

    void parser_init(struct http_parser *p);
    bool parse(struct http_parser *p, const char *data, size_t len);

    std::string data() const;
};

#endif // HTTP_HH
