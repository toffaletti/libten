#include "ten/error.hh"

#include <execinfo.h>
#include <cxxabi.h>
#include <sstream>
#include <memory>
#include <stdlib.h>

namespace ten {

saved_backtrace::saved_backtrace() {
    size = backtrace(array, 50);
}

std::string saved_backtrace::str() {
    std::stringstream ss;
    std::unique_ptr<char *, void (*)(void*)> messages{backtrace_symbols(array, size), free};
    // skip the first frame which is the constructor
    for (int i = 1; i < size && messages; ++i) {
        char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

        // find parantheses and +address offset surrounding mangled name
        for (char *p = messages.get()[i]; *p; ++p) {
            if (*p == '(') {
                mangled_name = p;
            } else if (*p == '+') {
                offset_begin = p;
            } else if (*p == ')') {
                offset_end = p;
                break;
            }
        }

        // if the line could be processed, attempt to demangle the symbol
        if (mangled_name && offset_begin && offset_end &&
            mangled_name < offset_begin)
        {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            std::unique_ptr<char, void (*)(void*)> real_name{abi::__cxa_demangle(mangled_name, 0, 0, &status), free};

            if (status == 0) {
                // if demangling is successful, output the demangled function name
                ss << "[bt]: (" << i << ") " << messages.get()[i] << " : "
                    << real_name.get() << "+" << offset_begin << offset_end
                    << std::endl;

            } else {
                // otherwise, output the mangled function name
                ss << "[bt]: (" << i << ") " << messages.get()[i] << " : "
                    << mangled_name << "+" << offset_begin << offset_end
                    << std::endl;
            }
        } else {
            // otherwise, print the whole line
            ss << "[bt]: (" << i << ") " << messages.get()[i] << std::endl;
        }
    }
    return ss.str();
}

void errorx::init(const char *msg, size_t len) {
    len = std::min(len, _bufsize - 1);
    memcpy(_buf, msg, len);
    _buf[len] = 0;
}

void errorx::initf(const char *fmt, ...) {
    _buf[0] = 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_buf, _bufsize, fmt, ap);
    va_end(ap);
}

void errno_error::_add_strerror() {
    // sometimes errno_error is a base when system errors are only possible,
    //   so append nothing if there is no error
    if (!_error)
        return;

    // requires GNU specific strerror_r
    // pointer might be ebuf or an internal static string
    char ebuf[64];
    const auto es = strerror_r(_error, ebuf, sizeof ebuf);
    const auto eslen = strlen(es);

    char *bp = strchr(_buf, '\0');
    char * const be = _buf + _bufsize - 1;
    if (bp != _buf) {
        if (bp < be) *bp++ = ':';
        if (bp < be) *bp++ = ' ';
    }
    const size_t escopy = std::min(eslen, static_cast<size_t>(be - bp));
    *std::copy(es, es + escopy, bp) = '\0';
}

} // end namespace ten

