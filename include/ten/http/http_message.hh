#ifndef LIBTEN_HTTP_MESSAGE_HH
#define LIBTEN_HTTP_MESSAGE_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <stdarg.h>
#include <boost/lexical_cast.hpp>

#include "http_parser.h"
#include "ten/task.hh"
#include "ten/error.hh"
#include "ten/optional.hh"

namespace ten {

bool ascii_iequals(const std::string &a, const std::string &b);

// TODO: define exceptions

typedef std::pair<std::string, std::string> header_pair;
typedef std::vector<header_pair> header_list;

// only a limited set of versions are supported
enum http_version {
    http_0_9,
    http_1_0,
    http_1_1,
    default_http_version = http_1_1
};
const std::string &version_string(http_version ver);

namespace hs {
//! common strings for HTTP
extern const std::string
    GET, HEAD, POST, PUT, DELETE,
    Connection, close, keep_alive,
    Host,
    Date,
    Accept,
    Accept_Charset,
    Accept_Encoding,
    Content_Length,
    Content_Type, text_plain, app_json, app_json_utf8, app_octet_stream,
    Content_Encoding, identity,
    Cache_Control, no_cache;
}

//! http statuses
enum http_status_t : uint16_t {
    HTTP_Continue                         = 100,
    HTTP_Switching_Protocols              = 101,
    HTTP_OK                               = 200,
    HTTP_Created                          = 201,
    HTTP_Accepted                         = 202,
    HTTP_Non_Authoritative_Information    = 203,
    HTTP_No_Content                       = 204,
    HTTP_Reset_Content                    = 205,
    HTTP_Partial_Content                  = 206,
    HTTP_Multiple_Choices                 = 300,
    HTTP_Moved_Permanently                = 301,
    HTTP_Found                            = 302,
    HTTP_See_Other                        = 303,
    HTTP_Not_Modified                     = 304,
    HTTP_Use_Proxy                        = 305,
    HTTP_Temporary_Redirect               = 307,
    HTTP_Bad_Request                      = 400,
    HTTP_Unauthroized                     = 401,
    HTTP_Payment_Required                 = 402,
    HTTP_Forbidden                        = 403,
    HTTP_Not_Found                        = 404,
    HTTP_Method_Not_Allowed               = 405,
    HTTP_Not_Acceptable                   = 406,
    HTTP_Proxy_Authentication_Required    = 407,
    HTTP_Request_Timeout                  = 408,
    HTTP_Conflict                         = 409,
    HTTP_Gone                             = 410,
    HTTP_Length_Required                  = 411,
    HTTP_Precondition_Failed              = 412,
    HTTP_Request_Entity_Too_Large         = 413,
    HTTP_Request_URI_Too_Large            = 414,
    HTTP_Unsupported_Media_Type           = 415,
    HTTP_Requested_Range_Not_Satisfiable  = 416,
    HTTP_Expectation_Failed               = 417,
    HTTP_Internal_Server_Error            = 500,
    HTTP_Not_Implemented                  = 501,
    HTTP_Bad_Gateway                      = 502,
    HTTP_Service_Unavailable              = 503,
    HTTP_Gateway_Timeout                  = 504,
    HTTP_Version_Not_Supported            = 505,
};

// an enum needs an explicit hash to be a map key
} // close ten
namespace std {
template <> struct hash<ten::http_status_t> {
    using argument_type = ten::http_status_t;
    using result_type   = size_t;
    result_type operator()(argument_type n) const noexcept {
        return static_cast<result_type>(n);
    }
};
} // close std
namespace ten {

//! http headers
struct http_headers {
protected:
    header_list _hlist;
    bool _got_header_field {}; // for reliable parsing
    friend struct http_header_parse; // visibility loophole for parsing

    header_list::iterator       _hfind(const std::string &field);
    header_list::const_iterator _hfind(const std::string &field) const;

    void _hwrite(std::ostream &os) const;

public:
    enum concat_t { concat };

    http_headers() {}

    template <typename ...Args>
        http_headers(Args&& ...args) {
            constexpr size_t hdr_fudge = 4; // room to grow
            constexpr auto n = sizeof...(args);
            static_assert(n % 2 == 0, "mismatched header name/value pairs");
            reserve((n / 2) + hdr_fudge);
            append(std::forward<Args>(args)...);
        }

    void append() {}

    template <typename ...Args>
        void append(concat_t, const http_headers &other, Args&& ...args) {
            if (&other != this)
                _hlist.insert(_hlist.end(), begin(other._hlist), end(other._hlist));  // no vector::append
            append(std::forward<Args>(args)...);
        }

    template <typename ValueT, typename ...Args>
        void append(const std::string &field, ValueT &&header_value,
                    const std::string &field2, Args&& ...args) {
            append(field, std::forward<ValueT>(header_value));
            append(field2, std::forward<Args>(args)...);
        }

    void append(const std::string &field, const std::string &value);

    template <typename ValueT>
        void append(const std::string &field, const ValueT &value) {
            append(field, boost::lexical_cast<std::string>(value));
        }

    void set(const std::string &field, const std::string &value);

    template <typename ValueT>
        void set(const std::string &field, const ValueT &value) {
            set(field, boost::lexical_cast<std::string>(value));
        }

    void clear() {
        _hlist.clear();
        _got_header_field = {};
    }

    void reserve(size_t n)  { _hlist.reserve(n); }
    bool empty() const;

    bool contains(const std::string &field) const;

    bool remove(const std::string &field);

    optional<std::string> get(const std::string &field) const;

    template <typename ValueT>
        optional<ValueT> get(const std::string &field) const {
            const auto i = _hfind(field);
            return (i == end(_hlist)) ? nullopt : optional<ValueT>{boost::lexical_cast<ValueT>(i->second)};
        }

#ifdef CHIP_UNSURE

    bool is(const std::string &field, const std::string &value) const;
    bool is_nocase(const std::string &field, const std::string &value) const;

    template <typename ValueT>
        bool is(const std::string &field, const ValueT &value) const {
            const auto i = _hfind(field);
            return (i != end(_hlist)) && (boost::lexical_cast<ValueT>(i->second) == value);
        }

#endif // CHIP_UNSURE
};

//! base class for http request and response
struct http_base : http_headers {
    using super = http_headers;

    http_version version {default_http_version};
    std::string body;
    size_t body_length {};
    bool complete {};

    explicit http_base(http_headers headers_ = {}, http_version version_ = default_http_version)
        : http_headers(std::move(headers_)), version{version_} {}

    void clear() {
        super::clear();
        version = default_http_version;
        body.clear();
        body_length = {};
        complete = {};
    }

    void set_body(std::string body_, const char *content_type) {
        set_body(std::move(body_), optional<std::string>(in_place, content_type));
    }
    void set_body(std::string body_, optional<std::string> content_type = nullopt) {
        body = std::move(body_);
        body_length = body.size();
        set(hs::Content_Length, body_length);
        if (content_type)
            set(hs::Content_Type, *content_type);
        else if (!contains(hs::Content_Type))
            set(hs::Content_Type, hs::text_plain);
    }

    bool close_after() const {
        const auto conn = get(hs::Connection);
        return version <= http_1_0
                 ? (!conn || !ascii_iequals(*conn, hs::keep_alive))
                 : (conn && ascii_iequals(*conn, hs::close));
    }

    // strftime can be quite expensive, so don't do it more than once per second
    static std::string rfc822_date() {
        static std::mutex last_mut;
        static time_t last_time = -1;
        static std::string last_date;

        // not using procnow() here because it isn't system_clock and only
        // system_clock can be converted to time_t
        time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::lock_guard<std::mutex> lk(last_mut);
        if (last_time != now) {
            char buf[64];
            struct tm tm;
            strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&now, &tm));
            last_time = now;
            last_date = buf;
        }
        return last_date;
    }
};

//! http request
struct http_request : http_base {
    using super = http_base;

    std::string method;
    std::string uri;

    http_request() : http_base() {}
    http_request(std::string method_,
                 std::string uri_,
                 http_headers headers_ = {},
                 http_version version_ = default_http_version)
        : http_base(std::move(headers_), version_),
          method{std::move(method_)},
          uri{std::move(uri_)}
    {}

    http_request(std::string method_,
                 std::string uri_,
                 http_headers headers_,
                 std::string body_,
                 optional<std::string> content_type_ = nullopt)
        : http_base(std::move(headers_)),
          method{std::move(method_)},
          uri{std::move(uri_)}
    { set_body(std::move(body_), std::move(content_type_)); }

    // variant for quoted string content type
    http_request(std::string method_,
                 std::string uri_,
                 http_headers headers_,
                 std::string body_,
                 const char *content_type_)
        : http_request(std::move(method_),
                       std::move(uri_),
                       std::move(headers_),
                       std::move(body_),
                       std::string(content_type_))
    {}

    void clear() {
        super::clear();
        method.clear();
        uri.clear();
    }

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;

    std::string path() const {
        std::string p = uri;
        const size_t pos = p.find_first_of("?#");
        if (pos != std::string::npos)
            p.erase(pos, std::string::npos);
        return p;
    }
};

//! http response
struct http_response : http_base {
    using super = http_base;
    using status_t = http_status_t;

    status_t status_code {};
    bool _parser_guillotine;  // return only the head during parsing

    http_response(status_t status_code_ = HTTP_OK,
                  http_headers headers_ = {},
                  http_version version_ = default_http_version)
        : http_base(std::move(headers_), version_),
          status_code{status_code_}
        {}

    http_response(status_t status_code_,
                  http_headers headers_,
                  std::string body_,
                  optional<std::string> content_type_ = nullopt)
        : http_base(std::move(headers_)),
          status_code{status_code_}
        { set_body(std::move(body_), std::move(content_type_)); }

    // variant for quoted string content type
    http_response(status_t status_code_,
                  http_headers headers_,
                  std::string body_,
                  const char *content_type_)
        : http_response(status_code_,
                        std::move(headers_),
                        std::move(body_),
                        std::string(content_type_))
        {}

    void clear() {
        super::clear();
        status_code = {};
    }

    const std::string &reason() const;

    void parser_init(struct http_parser *p, bool guillotine = false);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;
};

} // end namespace ten

#endif // LIBTEN_HTTP_MESSAGE_HH
