#include "ten/http/http_message.hh"
#include <sstream>
#include <algorithm>
#include <unordered_map>

namespace ten {

static const std::unordered_map<http_response::status_t, std::string> http_status_codes = {
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

namespace hs {
    extern const std::string
        GET{"GET"}, HEAD{"HEAD"}, POST{"POST"}, PUT{"PUT"}, DELETE{"DELETE"},
        Connection{"Connection"}, close{"close"}, keep_alive{"Keep-Alive"},
        Host{"Host"},
        Date{"Date"},
        Content_Length{"Content-Length"},
        Content_Type{"Content-Type"}, text_plain{"text/plain"}, app_octet_stream{"application/octet-stream"};
}

const std::string &version_string(http_version ver) {
    static const std::string v09{"HTTP/0.9"}, v10{"HTTP/1.0"}, v11{"HTTP/1.1"};
    switch (ver) {
      case http_0_9: return v09;
      case http_1_0: return v10;
      case http_1_1: return v11;
      default:       throw errorx("BUG: http_version %u", static_cast<unsigned>(ver));
    }
}

static void toupper_copy(char* dest, const char* str, size_t len)
{
    static uint8_t toupper_map[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
        17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
        40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,
        63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,
        86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,
        77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,
        130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,
        148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,
        166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,
        184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,
        202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,
        220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,
        238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
    size_t i;
    uint32_t eax, ebx;
    const uint8_t* ustr = (const uint8_t*) str;
    const size_t leftover = len % 4;
    const size_t imax = len / 4;
    const uint32_t* s = (const uint32_t*) str;
    uint32_t* d = (uint32_t*) dest;
    for (i = 0; i != imax; ++i) {
        eax = s[i];
        /*
         * This is based on the algorithm by Paul Hsieh
         * http://www.azillionmonkeys.com/qed/asmexample.html
         */
        ebx = (0x7f7f7f7fu & eax) + 0x05050505u;
        ebx = (0x7f7f7f7fu & ebx) + 0x1a1a1a1au;
        ebx = ((ebx & ~eax) >> 2)  & 0x20202020u;
        *d++ = eax - ebx;
    }

    i = imax*4;
    dest = (char*) d;
    switch (leftover) {
    case 3: *dest++ = (char) toupper_map[ustr[i++]];
    case 2: *dest++ = (char) toupper_map[ustr[i++]];
    case 1: *dest++ = (char) toupper_map[ustr[i]];
    case 0: *dest = '\0';
    }
}

bool ascii_iequals(const std::string &a, const std::string &b) {
    size_t len = a.size();
    if (len != b.size()) return false;
    // TODO: rewrite this to not allocate any memory
    std::unique_ptr<char[]> buf{new char[(len*2)+2]};
    char *aupper = buf.get();
    char *bupper = &buf.get()[len+1];
    toupper_copy(aupper, a.c_str(), len);
    toupper_copy(bupper, b.c_str(), len);
    return memcmp(aupper, bupper, len) == 0;
}

namespace {
struct is_header {
    const std::string &field;
    bool operator()(const header_pair &header) {
        return ascii_iequals(header.first, field);
    }
};
} // ns

header_list::iterator http_headers::_hfind(const std::string &field) {
    return std::find_if(begin(_hlist), end(_hlist), is_header{field});
}

header_list::const_iterator http_headers::_hfind(const std::string &field) const {
    return std::find_if(begin(_hlist), end(_hlist), is_header{field});
}

void http_headers::append(const std::string &field, const std::string &value) {
    _hlist.emplace_back(field, value);
}

void http_headers::set(const std::string &field, const std::string &value) {
    const auto i = _hfind(field);
    if (i != end(_hlist)) {
        i->second = value;
    } else {
        _hlist.emplace_back(field, value);
    }
}

bool http_headers::empty() const {
    return _hlist.empty();
}

bool http_headers::contains(const std::string &field) const {
    return _hfind(field) != end(_hlist);
}

bool http_headers::remove(const std::string &field) {
    auto i = std::remove_if(begin(_hlist), end(_hlist), is_header{field});
    if (i != end(_hlist)) {
        _hlist.erase(i, end(_hlist)); // remove now-invalid tail
        return true;
    }
    return false;
}

optional<std::string> http_headers::get(const std::string &field) const {
    auto i = _hfind(field);
    if (i != end(_hlist)) {
        return i->second;
    }
    return nullopt;
}

void http_headers::_hwrite(std::ostream &os) const {
    for (auto const &h : _hlist) {
        os << h.first << ": " << h.second << "\r\n";
    }
    os << "\r\n";
}

#ifdef CHIP_UNSURE

bool http_headers::is(const std::string &field, const std::string &value) const {
    const auto i = _hfind(field);
    return (i != hend()) && (i->second == value);
}

bool http_headers::is_nocase(const std::string &field, const std::string &value) const {
    const auto i = _hfind(field);
    return (i != hend()) && ascii_iequals(i->second, value);
}

#endif // CHIP_UNSURE

static bool set_version(http_version &ver, http_parser *p) {
    if      (p->http_major == 0 && p->http_minor == 9) ver = http_0_9;
    else if (p->http_major == 1 && p->http_minor == 0) ver = http_1_0;
    else if (p->http_major == 1 && p->http_minor == 1) ver = http_1_1;
    else return false;
    return true;
}

//! parser entry points
struct http_header_parse {
    static int field(http_headers *h, const char *at, size_t length) {
        if (!h->_got_header_field)
            h->_hlist.emplace_back();
        h->_hlist.back().first.append(at, length);
        h->_got_header_field = true;
        return 0;
    }
    static int value(http_headers *h, const char *at, size_t length) {
        assert(!h->_hlist.empty());
        h->_hlist.back().second.append(at, length);
        h->_got_header_field = false;
        return 0;
    }
};

extern "C" {

static int _on_header_field(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    return http_header_parse::field(m, at, length);
}

static int _on_header_value(http_parser *p, const char *at, size_t length) {
    http_base *m = reinterpret_cast<http_base *>(p->data);
    return http_header_parse::value(m, at, length);
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
    http_request *m = reinterpret_cast<http_request *>(p->data);
    m->uri.append(at, length);
    return 0;
}

static int _request_on_headers_complete(http_parser *p) {
    http_request *m = reinterpret_cast<http_request*>(p->data);
    m->method = http_method_str((http_method)p->method);
    if (!set_version(m->version, p)) {
        LOG(INFO) << "on_headers_complete: invalid version";
        return -1;
    }
    if (p->content_length > 0 && p->content_length != UINT64_MAX) {
        m->body.reserve(p->content_length);
    }
    return 0;
}

} // extern "C"


void http_request::parser_init(struct http_parser *p) {
    http_parser_init(p, HTTP_REQUEST);
    p->data = this;
    clear();
}

void http_request::parse(struct http_parser *p, const char *data_, size_t &len) {
    http_parser_settings s{};
    s.on_url              = _request_on_url;
    s.on_header_field     = _on_header_field;
    s.on_header_value     = _on_header_value;
    s.on_headers_complete = _request_on_headers_complete;
    s.on_body             = _on_body;
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
    std::ostringstream ss;
    ss << method << " " << uri << " " << version_string(version) << "\r\n";
    _hwrite(ss);
    return ss.str();
}

/* http_response_t */

static int _response_on_headers_complete(http_parser *p) {
    http_response *m = reinterpret_cast<http_response *>(p->data);
    m->status_code = p->status_code;
    if (!set_version(m->version, p)) {
        return -1;
    }
    if (p->content_length > 0 && p->content_length != UINT64_MAX) {
        m->body.reserve(p->content_length);
    }

    // if this is a response to a HEAD
    // we need to return 1 here so the
    // parser knowns not to expect a body
    return m->guillotine ? 1 : 0;
}

void http_response::parser_init(struct http_parser *p) {
    http_parser_init(p, HTTP_RESPONSE);
    p->data = this;
    clear();
}

void http_response::parse(struct http_parser *p, const char *data_, size_t &len) {
    http_parser_settings s{};
    s.on_header_field     = _on_header_field;
    s.on_header_value     = _on_header_value;
    s.on_headers_complete = _response_on_headers_complete;
    s.on_body             = _on_body;
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
    static const std::string unknown{"Unknown"};
    return unknown;
}

std::string http_response::data() const {
    std::ostringstream ss;
    ss << version_string(version) << " " << status_code << " " << reason() << "\r\n";
    _hwrite(ss);
    return ss.str();
}

} // end namespace ten

