#ifndef LIBTEN_HTTP_ERROR_HH
#define LIBTEN_HTTP_ERROR_HH

#include "ten/error.hh"

namespace ten {

class http_error : public errno_error {
public:
    // TODO: variadic or inherited ctors
    //! captures errno implicitly
    http_error(const char *msg) : errno_error(msg) {}
    //! when you don't want to capture errno implicitly, dial and close use this
    http_error(int err, const char *msg) : errno_error(err, msg) {}
};

//! thrown on http dial errors, which always come with specific messages
struct http_dial_error : public http_error {
    http_dial_error(const char *msg) : http_error(0, msg) {}
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

} // end namespace ten

#endif // LIBTEN_HTTP_ERROR_HH
