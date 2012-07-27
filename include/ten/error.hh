#ifndef LIBTEN_ERROR_HH
#define LIBTEN_ERROR_HH

#include <exception>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <string.h>
#include <errno.h>

namespace ten {

//! capture the current stack trace
struct saved_backtrace {
    void *array[50];
    int size;

    saved_backtrace();
    std::string str();
};

//! captures the backtrace in the constructor
//
//! useful for coroutines because the stack is lost switching contexts
class backtrace_exception : public std::exception {
public:
    std::string backtrace_str() { return bt.str(); }
private:
    saved_backtrace bt;
};

//! exception that sets what() based on current errno value
struct errno_error : backtrace_exception {
private:
    //! the value of errno when this exception was created
    int _error;
    char _buf[128];
    const char *_what;

public:
    //! \param err the error as specified by errno
    errno_error(int err = errno) : _error(err) {
        // requires GNU specific strerror_r
        // _what might be _buf or an internal static string
        _what = strerror_r(_error, _buf, sizeof(_buf));
    }
    errno_error(const errno_error &ee) { copy(ee); }
    errno_error & operator = (const errno_error &ee) { copy(ee); return *this; }

    //! \return string result from strerror_r
    const char *what() const noexcept override { return _what; }

private:
    void copy(const errno_error &ee) {
        _error = ee._error;
        memcpy(_buf, ee._buf, sizeof _buf);
        _what = (ee._what == ee._buf) ? _buf : ee._what;
    }
};

//! construct a what() string in printf() format
struct errorx : backtrace_exception {
private:
    char _buf[256];

public:
    //! \param fmt printf-style format string
    errorx(const char *fmt, ...) __attribute__((format (printf, 2, 3))) {
        _buf[0] = 0;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(_buf, sizeof(_buf), fmt, ap);
        va_end(ap);
    }

    errorx(const std::string &msg) {
        strncpy(_buf, msg.data(), sizeof(_buf)-1);
        _buf[sizeof(_buf)-1] = 0;
    }

    //! \return a string describing the error
    const char *what() const noexcept override { return _buf; }
};

//! macro to throw errno_error if exp returns -1
#define THROW_ON_ERROR(exp) \
    if ((exp) == -1) { \
        throw errno_error(); \
    }

//! macro to throw errno_error if exp returns NULL
#define THROW_ON_NULL(exp) \
    if ((exp) == NULL) { \
        throw errno_error(); \
    }

//! macro to throw errno_error if exp != 0
#define THROW_ON_NONZERO_ERRNO(exp) \
    do { \
        int _rv = (exp); \
        if (_rv != 0) throw errno_error(_rv); \
    } while (0)

} // end namespace ten

#endif // LIBTEN_ERROR_HH
