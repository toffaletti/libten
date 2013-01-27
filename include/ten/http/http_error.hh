#ifndef LIBTEN_HTTP_ERROR_HH
#define LIBTEN_HTTP_ERROR_HH

#include "ten/error.hh"

namespace ten {

class http_error : public errorx {
public:
    // TODO: variadic or inherited ctors
    http_error(const char *msg) : errorx(msg) {}
};

class http_errno_error : public errno_error {
protected:
    // TODO: variadic or inherited ctors
    http_errno_error(const char *msg) : errno_error(msg) {}
};

//! thrown on http dial errors, which always come with specific messages
struct http_dial_error : public http_error {
    http_dial_error(const char *msg) : http_error(msg) {}
};

//! thrown on http network errors
struct http_makesock_error : public http_errno_error {
    http_makesock_error() : http_errno_error("makesock") {}
};

//! thrown on http network errors
struct http_send_error : public http_errno_error {
    http_send_error() : http_errno_error("send") {}
};

//! thrown on http network errors
struct http_recv_error : public http_errno_error {
    http_recv_error() : http_errno_error("recv") {}
};

//! thrown on http network errors
struct http_closed_error : public http_error {
    http_closed_error() : http_error("closed") {}
};

} // end namespace ten

#endif // LIBTEN_HTTP_ERROR_HH
