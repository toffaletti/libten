#include "http_message.hh"
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <algorithm>

// normalize message header field names.
static std::string normalize_header_name(const std::string &field) {
    bool first_letter = true;
    std::stringstream ss;
    for (std::string::const_iterator i = field.begin(); i!=field.end(); ++i) {
        if (!first_letter) {
            char c = tolower(*i);
            if (c == '_') {
                c = '-';
                first_letter = true;
            } else if (c == '-') {
                first_letter = true;
            }
            ss << c;
        } else {
            ss << (char)toupper(*i);
            first_letter = false;
        }
    }
    return ss.str();
}

struct is_header {
    const std::string &field;
    is_header(const std::string &normal_field) : field(normal_field) {}

    bool operator()(const std::pair<std::string, std::string> &header) {
        return header.first == field;
    }
};

void http_base::append_header(const std::string &field, const std::string &value) {
    std::string normal_field = normalize_header_name(field);
    headers.push_back(std::make_pair(normal_field, value));
}

void http_base::append_header(const std::string &field, unsigned long long value) {
    append_header(field, boost::lexical_cast<std::string>(value));
}

bool http_base::remove_header(const std::string &field) {
    std::string normal_field = normalize_header_name(field);
    header_list::iterator i = std::remove_if(headers.begin(),
        headers.end(), is_header(normal_field));
    if (i != headers.end()) {
        headers.erase(i, headers.end()); // remove *all*
        return true;
    }
    return false;
}

std::string http_base::header_string(const std::string &field) {
    header_list::iterator i = std::find_if(headers.begin(),
        headers.end(), is_header(normalize_header_name(field)));
    if (i != headers.end()) {
        return i->second;
    }
    return "";
}

unsigned long long http_base::header_ull(const std::string &field) {
    header_list::iterator i = std::find_if(headers.begin(),
        headers.end(), is_header(normalize_header_name(field)));
    if (i != headers.end()) {
        return boost::lexical_cast<unsigned long long>(i->second);
    }
    return 0;
}

void http_base::normalize_headers() {
    for (header_list::iterator i=headers.begin(); i!=headers.end(); ++i) {
        i->first.assign(normalize_header_name(i->first));
    }
}

static int _on_header_field(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    if (m->headers.empty()) {
        m->headers.push_back(std::make_pair("", ""));
    } else if (!m->headers.back().second.empty()) {
        m->headers.push_back(std::make_pair("", ""));
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
    return 0;
}

static int _request_on_url(http_parser *p, const char *at, size_t length) {
    http_request *m = (http_request *)p->data;
    m->uri.append(at, length);
    return 0;
}

static int _request_on_headers_complete(http_parser *p) {
    http_request *m = reinterpret_cast<http_request*>(p->data);
    m->normalize_headers();
    m->method = http_method_str((http_method)p->method);
    std::stringstream ss;
    ss << "HTTP/" << p->http_major << "." << p->http_minor;
    m->http_version = ss.str();

    if (p->content_length > 0) {
        m->body.reserve(p->content_length);
    }

    return 0;
}

void http_request::parser_init(struct http_parser *p) {
    http_parser_init(p, HTTP_REQUEST);
    p->data = this;
    clear();
}

bool http_request::parse(struct http_parser *p, const char *data, size_t len) {
    http_parser_settings s;
    s.on_message_begin = NULL;
    s.on_url = _request_on_url;
    s.on_reason = NULL;
    s.on_header_field = _on_header_field;
    s.on_header_value = _on_header_value;
    s.on_headers_complete = _request_on_headers_complete;
    s.on_body = _on_body;
    s.on_message_complete = _on_message_complete;

    ssize_t nparsed = http_parser_execute(p, &s, data, len);
    if (nparsed != (ssize_t)len) {
        throw fw::errorx("%s: %s",
            http_errno_name((http_errno)p->http_errno),
            http_errno_description((http_errno)p->http_errno));
    }
    return complete;
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

static int _response_on_reason(http_parser *p, const char *at, size_t length) {
    http_response *m = (http_response *)p->data;
    m->reason.append(at, length);
    return 0;
}

static int _response_on_headers_complete(http_parser *p) {
    http_response *m = reinterpret_cast<http_response *>(p->data);
    m->normalize_headers();
    m->status_code = p->status_code;
    std::stringstream ss;
    ss << "HTTP/" << p->http_major << "." << p->http_minor;
    m->http_version = ss.str();

    if (p->content_length > 0) {
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

bool http_response::parse(struct http_parser *p, const char *data, size_t len) {
    http_parser_settings s;
    s.on_message_begin = NULL;
    s.on_url = NULL;
    s.on_reason = _response_on_reason;
    s.on_header_field = _on_header_field;
    s.on_header_value = _on_header_value;
    s.on_headers_complete = _response_on_headers_complete;
    s.on_body = _on_body;
    s.on_message_complete = _on_message_complete;

    ssize_t nparsed = http_parser_execute(p, &s, data, len);
    if (nparsed != (ssize_t)len) {
        throw fw::errorx("%s: %s",
            http_errno_name((http_errno)p->http_errno),
            http_errno_description((http_errno)p->http_errno));
    }
    return complete;
}

std::string http_response::data() const {
    std::stringstream ss;
    ss << http_version << " " << status_code << " " << reason << "\r\n";
    for (header_list::const_iterator i = headers.begin(); i!=headers.end(); ++i) {
        ss << i->first << ": " << i->second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

