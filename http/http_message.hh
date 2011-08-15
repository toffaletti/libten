#ifndef HTTP_HH
#define HTTP_HH

#include <string>
#include <vector>

#include "http_request_parser.h"
#include "http_response_parser.h"

typedef std::pair<std::string, std::string> header_pair;
typedef std::vector<header_pair> header_list;

struct http_base {
    header_list headers;

    void append_header(const std::string &field, const std::string &value);
    void append_header(const std::string &field, unsigned long long value);
    header_list::iterator find_header(const std::string &field);
    bool remove_header(const std::string &field);
    std::string header_string(const std::string &field);
    unsigned long long header_ull(const std::string &field);
};

struct http_request : http_base {
    std::string method;
    std::string uri;
    std::string fragment;
    std::string path;
    std::string query_string;
    std::string http_version;
    std::string body;
    size_t body_length;

    http_request() : body_length(0) {}
    http_request(const std::string &method_,
        const std::string &uri_,
        const std::string &http_version_ = "HTTP/1.1")
        : method(method_), uri(uri_), http_version(http_version_) {}

    void clear() {
        headers.clear();
        method.clear();
        uri.clear();
        fragment.clear();
        path.clear();
        query_string.clear();
        http_version.clear();
        body.clear();
        body_length = 0;
    }

    void parser_init(http_request_parser *p);

    std::string data();
};

struct http_response : http_base {
    std::string http_version;
    unsigned long status_code;
    std::string reason;
    std::string body;
    size_t body_length;
    size_t chunk_size;
    bool last_chunk;

    http_response(unsigned long status_code_ = 200,
        const std::string &reason_ = "OK",
        const std::string &http_version_ = "HTTP/1.1")
        : http_version(http_version_),
        status_code(status_code_),
        reason(reason_),
        body_length(0), chunk_size(0), last_chunk(false) {}

    void clear() {
        headers.clear();
        http_version.clear();
        status_code = 0;
        reason.clear();
        body.clear();
        body_length = 0;
        chunk_size = 0;
        last_chunk = false;
    }

    void parser_init(http_response_parser *p);

    std::string data();

    void set_body(const std::string &body_) {
        body = body_;
        body_length = body.size();
        remove_header("Content-Length");
        append_header("Content-Length", body_length);
    }
};

#endif // HTTP_HH
