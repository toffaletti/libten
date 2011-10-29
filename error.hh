#ifndef ERROR_HH
#define ERROR_HH

#include <exception>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <string.h>
#include <errno.h>

namespace ten {

//! captures the backtrace in the constructor
//
//! useful for coroutines because the stack is lost switching contexts
class backtrace_exception : public std::exception {
public:
    backtrace_exception();

    std::string str();
private:
    void *array[50];
    int size;
};

//! exception that sets what() based on current errno value
struct errno_error : backtrace_exception {
    char _buf[256];
    const char *_what;
    //! the value of errno when this exception was created
    int _error;

    //! \param err the error as specified by errno
    errno_error(int err = errno) : _error(err) {
        // requires GNU specific strerror_r
        // _what might be _buf or an internal static string
        _what = strerror_r(_error, _buf, sizeof(_buf));
    }

    //! \return string result from strerror_r
    const char *what() const throw() { return _what; }
};

//! construct a what() string in printf() format
struct errorx : backtrace_exception {
    char _buf[256];

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

} // end namespace ten

#endif // ERROR_HH

