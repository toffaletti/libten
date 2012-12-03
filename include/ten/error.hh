#ifndef LIBTEN_ERROR_HH
#define LIBTEN_ERROR_HH

#include <exception>
#include <algorithm>
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

    saved_backtrace() {
        capture();
    }

    void capture();
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

//! construct a what() string in printf() format
struct errorx : backtrace_exception {
protected:
    static constexpr size_t _bufsize = 256;
    char _buf[_bufsize];

    void init(const char *msg, size_t len);
    void initf(const char *fmt, ...) __attribute__((format (printf, 2, 3)));

    errorx() { _buf[0] = '\0'; }

public:
    errorx(const std::string &msg) { init(msg.data(), msg.size()); }
    errorx(const char *msg)        { init(msg, strlen(msg)); }

    //! \param fmt printf-style format string
    template <typename A, typename... Args>
    errorx(const char *fmt, A &&a, Args&&... args) { initf(fmt, std::forward<A>(a), std::forward<Args>(args)...); }

    //! \return a string describing the error
    const char *what() const noexcept override { return _buf; }
};

// helper to make sure errno is saved first, below
class errno_saver {
    friend class errno_error;
    const int _error;
    errno_saver(const int e = errno) : _error(e) {}
};

//! exception that sets what() based on current errno value
struct errno_error : private errno_saver, public errorx {
public:
    //! \param err the error as specified by errno
    errno_error() { _add_strerror(); }

    errno_error(const errno_error &) = default;
    errno_error(errno_error &&e) = default;
    errno_error(errno_error &e) : errno_saver(e), errorx(e) {}

    template <typename... Args>
    errno_error(Args&&... args)
        : errorx(std::forward<Args>(args)...) { _add_strerror(); }

    template <typename... Args>
    errno_error(const int err, Args&&... args)
        : errno_saver(err), errorx(std::forward<Args>(args)...) { _add_strerror(); }

    int error() const { return _error; }

private:
    void _add_strerror();
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
