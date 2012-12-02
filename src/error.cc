#include "ten/error.hh"

#include <execinfo.h>
#include <cxxabi.h>
#include <sstream>
#include <memory>
#include <stdlib.h>

namespace ten {

void saved_backtrace::capture() {
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

} // end namespace ten

