#ifndef LIBTEN_HTTP_ERROR_HH
#define LIBTEN_HTTP_ERROR_HH

#include "ten/error.hh"

namespace ten {

struct http_error : public errno_error {
    template <typename... Args>
    http_error(Args&&... args)
        : errno_error(std::forward<Args>(args)...) {}
};

//! thrown on http dial errors, which always come with specific messages,
//! and sometimes with errno values
struct http_dial_error : public http_error {
    http_dial_error(const char *msg) : http_error(0, msg) {}
    http_dial_error(const errno_error &e) : http_error(e) {}
    http_dial_error(int err, const char *msg) : http_error(err, msg) {}
};

//! thrown on http network errors
struct http_makesock_error : public http_error {
    http_makesock_error() : http_error("makesock") {}
};

//! thrown on http network errors
struct http_send_error : public http_error {
    http_send_error() : http_error("send") {}
};

//! thrown on http network errors
struct http_recv_error : public http_error {
    http_recv_error() : http_error("recv") {}
};

//! thrown on http network errors
struct http_closed_error : public http_error {
    http_closed_error() : http_error(0, "closed") {}
};

//! thrown on http parsing errors
struct http_parse_error : public http_error {
    template <typename... A>
    http_parse_error(A&&... a) : http_error(0, std::forward<A>(a)...) {}
};

} // end namespace ten

#endif // LIBTEN_HTTP_ERROR_HH
