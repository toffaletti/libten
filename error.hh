#ifndef ERROR_HH
#define ERROR_HH

#include <exception>
#include <cstdarg>
#include <cstdio>
#include <string.h>
#include <errno.h>

//! exception that sets what() based on current errno value
struct errno_error : std::exception {
    char _buf[256];
    const char *_what;
    //! the value of errno when this exception was created
    int error;

    //! \param err the error as specified by errno
    errno_error(int err = errno) : error(err) {
        // requires GNU specific strerror_r
        // _what might be _buf or an internal static string
        _what = strerror_r(error, _buf, sizeof(_buf));
    }

    //! \return string result from strerror_r
    const char *what() const throw() { return _what; }
};

//! construct a what() string in printf() format
struct error : std::exception {
    char _buf[256];

    //! \param fmt printf-style format string
    error(const char *fmt, ...) __attribute__((format (printf, 2, 3))) {
        _buf[0] = 0;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(_buf, sizeof(_buf), fmt, ap);
        va_end(ap);
    }

    //! \return a string describing the error
    const char *what() const throw () { return _buf; }
};

//! macro to throw if exp returns -1
#define THROW_ON_ERROR(exp) \
    if ((exp) == -1) { \
        throw errno_error(); \
    }

#define THROW_ON_NULL(exp) \
    if ((exp) == NULL) { \
        throw errno_error(); \
    }

#define THROW_ON_NONZERO(exp) \
    { \
        int _rv = (exp); \
        if (_rv != 0) throw errno_error(_rv); \
    }

#endif // ERROR_HH

