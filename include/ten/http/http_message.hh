#ifndef LIBTEN_HTTP_MESSAGE_HH
#define LIBTEN_HTTP_MESSAGE_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <stdarg.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "http_parser.h"
#include "ten/task.hh"
#include "ten/error.hh"
#include "ten/optional.hh"

namespace ten {

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
    Content_Length,
    Content_Type, text_plain, app_octet_stream;
}

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

    bool contains(const std::string &field) const;

    bool remove(const std::string &field);

    optional<std::string> get(const std::string &field) const;

    template <typename ValueT>
        optional<ValueT> get(const std::string &field) const {
            const auto i = find(field);
            return (i == end(headers)) ? nullopt : optional<ValueT>{boost::lexical_cast<ValueT>(i->second)};
        }

#ifdef CHIP_UNSURE

    bool is(const std::string &field, const std::string &value) const;
    bool is_nocase(const std::string &field, const std::string &value) const;

    template <typename ValueT>
        bool is(const std::string &field, const ValueT &value) const {
            const auto i = find(field);
            return (i != end(headers)) && (boost::lexical_cast<ValueT>(i->second) == value);
        }

#endif // CHIP_UNSURE

protected:
    // iterator-based access requires knowledge of value_type, so not public
    header_list::iterator       find(const std::string &field);
    header_list::const_iterator find(const std::string &field) const;
};

//! base class for http request and response
struct http_base : http_headers {
    http_version version {default_http_version};
    std::string body;
    size_t body_length {};
    bool complete {};

    explicit http_base(http_headers headers_ = {}, http_version version_ = default_http_version)
        : http_headers(std::move(headers_)), version{version_} {}

    void clear() {
        headers.clear();
        version = default_http_version;
        body.clear();
        body_length = {};
        complete = {};
    }

    void set_body(std::string body_,
                  const std::string &content_type_ = std::string())
    {
        body = std::move(body_);
        body_length = body.size();
        set(hs::Content_Length, body_length);
        remove(hs::Content_Type);
        if (!content_type_.empty()) {
            append(hs::Content_Type, content_type_);
        }
    }

    bool close_after() const {
        const auto conn = get(hs::Connection);
        return version <= http_1_0
                 ? (!conn || !boost::iequals(*conn, hs::keep_alive))
                 : (conn && boost::iequals(*conn, hs::close));
    }

    // strftime can be quite expensive, so don't do it more than once per second
    static std::string rfc822_date() {
        static std::mutex last_mut;
        static time_t last_time = -1;
        static std::string last_date;

        time_t now = std::chrono::system_clock::to_time_t(procnow());
        std::lock_guard<std::mutex> lk(last_mut);
        if (last_time != now) {
            char buf[128];
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
                 std::string content_type_ = std::string())
        : http_base(std::move(headers_)),
          method{std::move(method_)},
          uri{std::move(uri_)}
    { set_body(std::move(body_), std::move(content_type_)); }

    void clear() {
        http_base::clear();
        method.clear();
        uri.clear();
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
    using status_t = uint16_t;
    status_t status_code {};
    bool guillotine {};  // return only the head

    http_response(status_t status_code_ = 200,
                  http_headers headers_ = {},
                  http_version version_ = default_http_version)
        : http_base(std::move(headers_), version_),
          status_code{status_code_}
        {}
    http_response(status_t status_code_,
                  http_headers headers_,
                  std::string body_,
                  std::string content_type_ = std::string())
        : http_base(std::move(headers_)),
          status_code{status_code_}
        { set_body(std::move(body_), std::move(content_type_)); }

    // TODO: remove this special case
    http_response(http_request *req_)
        : http_base(),
          guillotine{req_ && req_->method == hs::HEAD}
        {}

    void clear() {
        http_base::clear();
        status_code = {};
        guillotine = {};
    }

    const std::string &reason() const;

    void parser_init(struct http_parser *p);
    void parse(struct http_parser *p, const char *data, size_t &len);

    std::string data() const;
};

} // end namespace ten

#endif // LIBTEN_HTTP_MESSAGE_HH
