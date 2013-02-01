#ifndef TEN_JSONSTREAM_HH
#define TEN_JSONSTREAM_HH

#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <stack>
#include <type_traits>
#include <cmath>
#include <string.h>

namespace ten {

namespace jsonstream_manip {
    namespace detail {
        struct jsobject {};
        struct jsarray {};
        struct jsend {};
        struct jsescape {};
        struct jsnoescape {};
        struct jsraw {};
        struct jsnoraw {};
    }

    extern detail::jsobject jsobject;
    extern detail::jsarray jsarray;
    extern detail::jsend jsend;
    extern detail::jsescape jsescape;
    extern detail::jsnoescape jsnoescape;
    extern detail::jsraw jsraw;
    extern detail::jsnoraw jsnoraw;
}

struct jsonstream_error : public std::runtime_error {
    jsonstream_error(const char *s) : std::runtime_error(s) {}
};

//! specialize for types that are OK to be json object keys
template <class T>
struct can_json_key : std::false_type {};

template<>
struct can_json_key<std::string> : std::true_type {};

template<>
struct can_json_key<const char *> : std::true_type {};

template<>
struct can_json_key<char *> : std::true_type {};

template <class Iterator>
void json_escape_string(std::ostream &o, Iterator iter, const Iterator &stop) {
    for (; iter != stop; iter++) {
        switch (*iter) {
            case '\\': o << "\\\\"; break;
            case  '"': o << "\\\""; break;
            case  '/': o << "\\/"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default  : o << *iter; break;
        }
    }
}

//! dangerously fast construction of json output, maybe
// XXX: you probably don't want to use this. see json.hh
struct jsonstream {
    enum class state {
        none,
        object_begin,
        object_key,
        object_value,
        array_begin,
        array_value,
    };

    std::ostream &os;
    std::stack<state> states;
    bool escape = false;
    bool raw = false;

    jsonstream(std::ostream &os_) : os (os_) {
        os << std::boolalpha;
        os << std::showpoint;
        states.emplace(state::none);
    }

    jsonstream &operator<<(const std::string &s) {
        transition(s, [&] {
            if (raw) {
                os << s;
            } else {
                os << "\"";
                if (escape) {
                    json_escape_string(os, begin(s), end(s));
                } else {
                    os << s;
                }
                os << "\"";
            }
        });
        return *this;
    }

    jsonstream &operator<<(const char *s) {
        transition(s, [&] {
            if (raw) {
                os << s;
            } else {
                os << "\"";
                if (escape) {
                    json_escape_string(os, s, s+strlen(s));
                } else {
                    os << s;
                }
                os << "\"";
            }
        });
        return *this;
    }

    jsonstream &operator<<(bool b) {
        transition(b, [&] {
            os << b;
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_floating_point<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(number, [&] {
            if (std::isnan(number)) {
                os << "null";
            } else if (std::isinf(number)) {
                os << "null";
            } else {
                char buf[32];
                snprintf(buf, sizeof(buf), "%g", number);
                os << buf;
            }
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_integral<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(number, [&] {
            os << static_cast<
                typename std::conditional<std::is_signed<T>::value, intmax_t, uintmax_t>::type
                >(number);
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsraw mip) {
        raw = true;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsnoraw mip) {
        raw = false;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsescape mip) {
        escape = true;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsnoescape mip) {
        escape = false;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsobject mip) {
        transition(mip, [&] {
            states.emplace(state::object_begin);
            os << "{";
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsarray mip) {
        transition(mip, [&] {
            states.emplace(state::array_begin);
            os << "[";
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::jsend mip) {
        switch (states.top()) {
            case state::object_begin:
                states.pop(); // empty object
                os << "}";
                break;
            case state::object_value:
                states.pop();
                if (states.top() != state::object_begin) {
                    throw jsonstream_error("bad state");
                }
                states.pop();
                os << "}";
                break;
            case state::array_begin:
                states.pop(); // empty array
                os << "]";
                break;
            case state::array_value:
                states.pop();
                if (states.top() != state::array_begin) {
                    throw jsonstream_error("bad state");
                }
                states.pop();
                os << "]";
                break;
            default:
                throw jsonstream_error("bad state");
        }
        return *this;
    }

    template <class T, class Func>
    void transition(const T&, Func &&f) {
        switch (states.top()) {
            case state::none:
                f();
                break;
            case state::object_begin:
                if (!can_json_key<T>::value) {
                    throw jsonstream_error("invalid key");
                }
                states.emplace(state::object_key);
                f();
                os << ":";
                break;
            case state::object_key:
                states.top() = state::object_value;
                f();
                break;
            case state::object_value:
                if (!can_json_key<T>::value) {
                    throw jsonstream_error("invalid key");
                }
                states.top() = state::object_key;
                os << ",";
                f();
                os << ":";
                break;
            case state::array_begin:
                states.emplace(state::array_value);
                f();
                break;
            case state::array_value:
                os << ",";
                f();
                break;
        }
    }
};

#if 0
template <class T, typename = typename T::key_type> struct is_associative : public std::true_type {};

template <class AssociativeContainer,
         typename = typename AssociativeContainer::key_type,
         typename = typename AssociativeContainer::value_type>
jsonstream &operator <<(jsonstream &o, const AssociativeContainer &seq) {
    using namespace jsonstream_manip;
    o << jsobject;
    for (auto i = std::begin(seq); i != std::end(seq); ++i) {
        o << i->first << i->second;
    }
    o << jsend;
    return o;
}
#endif

template <class SequenceContainer, typename = typename SequenceContainer::value_type>
jsonstream &operator <<(jsonstream &o, const SequenceContainer &seq) {
    using namespace jsonstream_manip;
    o << jsarray;
    for (auto i = std::begin(seq); i != std::end(seq); ++i) {
        o << *i;
    }
    o << jsend;
    return o;
}

} // ten

#endif
