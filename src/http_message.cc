#include "ten/http/http_message.hh"
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <boost/algorithm/string.hpp>

namespace ten {

extern const std::string http_1_0{"HTTP/1.0"};
extern const std::string http_1_1{"HTTP/1.1"};

static std::unordered_map<uint16_t, std::string> http_status_codes = {
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 307, "Temporary Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthroized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Time-out" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Request Entity Too Large" },
    { 414, "Request-URI Too Large" },
    { 415, "Unsupported Media Type" },
    { 416, "Requested range not satisfiable" },
    { 417, "Expectation Failed" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
};

namespace {
struct is_header {
    const std::string &field;
    bool operator()(const std::pair<std::string, std::string> &header) {
        return boost::iequals(header.first, field);
    }
};
} // ns

void http_headers::set(const std::string &field, const std::string &value) {
    auto i = std::find_if(headers.begin(), headers.end(), is_header{field});
    if (i != headers.end()) {
        i->second = value;
    } else {
        headers.push_back(std::make_pair(field, value));
    }
}

void http_headers::append(const std::string &field, const std::string &value) {
    headers.push_back(std::make_pair(field, value));
}

bool http_headers::remove(const std::string &field) {
    auto i = std::remove_if(headers.begin(), headers.end(), is_header{field});
    if (i != headers.end()) {
        headers.erase(i, headers.end()); // remove now-invalid tail
        return true;
    }
    return false;
}

std::string http_headers::get(const std::string &field) const {
    auto i = std::find_if(headers.begin(), headers.end(), is_header{field});
    if (i != headers.end()) {
        return i->second;
    }
    return std::string();
}

static int _on_header_field(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    if (m->headers.empty()) {
        m->headers.push_back(std::make_pair(std::string(), std::string()));
    } else if (!m->headers.back().second.empty()) {
        m->headers.push_back(std::make_pair(std::string(), std::string()));
    }
    m->headers.back().first.append(at, length);
    return 0;
}

static int _on_header_value(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    m->headers.back().second.append(at, length);
    return 0;
}

static int _on_body(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    m->body.append(at, length);
    return 0;
}

static int _on_message_complete(http_parser *p) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    m->complete = true;
    m->body_length = m->body.size();
    return 1; // cause parser to exit, this http_message is complete
}

static int _request_on_url(http_parser *p, const char *at, size_t length) {
    http_request *m = (http_request *)p->data;
    m->uri.append(at, length);
    return 0;
}

static int _request_on_headers_complete(http_parser *p) {
    http_request *m = reinterpret_cast<http_request*>(p->data);
    m->method = http_method_str((http_method)p->method);
    std::stringstream ss;
    ss << "HTTP/" << p->http_major << "." << p->http_minor;
    m->http_version = ss.str();

    if (p->content_length > 0 && p->content_length != ULLONG_MAX) {
        m->body.reserve(p->content_length);
    }

    return 0;
}

void http_request::parser_init(struct http_parser *p) {
    http_parser_init(p, HTTP_REQUEST);
    p->data = this;
    clear();
}

void http_request::parse(struct http_parser *p, const char *data_, size_t &len) {
    http_parser_settings s;
    s.on_message_begin = NULL;
    s.on_url = _request_on_url;
    s.on_header_field = _on_header_field;
    s.on_header_value = _on_header_value;
    s.on_headers_complete = _request_on_headers_complete;
    s.on_body = _on_body;
    s.on_message_complete = _on_message_complete;

    ssize_t nparsed = http_parser_execute(p, &s, data_, len);
    if (!complete && nparsed != (ssize_t)len) {
        len = nparsed;
        throw errorx("%s: %s",
            http_errno_name((http_errno)p->http_errno),
            http_errno_description((http_errno)p->http_errno));
    }
    len = nparsed;
}

std::string http_request::data() const {
    std::stringstream ss;
    ss << method << " " << uri << " " << http_version << "\r\n";
    for (header_list::const_iterator i = headers.begin(); i!=headers.end(); ++i) {
        ss << i->first << ": " << i->second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

/* http_response_t */

static int _response_on_headers_complete(http_parser *p) {
    http_response *m = reinterpret_cast<http_response *>(p->data);
    m->status_code = p->status_code;
    std::stringstream ss;
    ss << "HTTP/" << p->http_major << "." << p->http_minor;
    m->http_version = ss.str();

    if (p->content_length > 0 && p->content_length != ULLONG_MAX) {
        m->body.reserve(p->content_length);
    }

    // if this is a response to a HEAD
    // we need to return 1 here so the
    // parser knowns not to expect a body
    if (m->req && m->req->method == "HEAD") {
        return 1;
    }
    return 0;
}

void http_response::parser_init(struct http_parser *p) {
    http_parser_init(p, HTTP_RESPONSE);
    p->data = this;
    clear();
}

void http_response::parse(struct http_parser *p, const char *data_, size_t &len) {
    http_parser_settings s;
    s.on_message_begin = NULL;
    s.on_url = NULL;
    s.on_header_field = _on_header_field;
    s.on_header_value = _on_header_value;
    s.on_headers_complete = _response_on_headers_complete;
    s.on_body = _on_body;
    s.on_message_complete = _on_message_complete;

    ssize_t nparsed = http_parser_execute(p, &s, data_, len);
    if (!complete && nparsed != (ssize_t)len) {
        len = nparsed;
        throw errorx("%s: %s",
            http_errno_name((http_errno)p->http_errno),
            http_errno_description((http_errno)p->http_errno));
    }
    len = nparsed;
}


const std::string &http_response::reason() const {
    auto i = http_status_codes.find(status_code);
    if (i != http_status_codes.end()) {
        return i->second;
    }
    static std::string unknown = "Unknown";
    return unknown;
}

std::string http_response::data() const {
    std::stringstream ss;
    ss << http_version << " " << status_code << " " << reason() << "\r\n";
    for (header_list::const_iterator i = headers.begin(); i!=headers.end(); ++i) {
        ss << i->first << ": " << i->second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

} // end namespace ten

