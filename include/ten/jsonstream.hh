#ifndef TEN_JSONSTREAM_HH
#define TEN_JSONSTREAM_HH

#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <stack>
#include <type_traits>
#include <functional>
#include <cmath>
#include <string.h>

namespace ten {

namespace jsonstream_manip {
    enum jsobject_t   { jsobject };
    enum jsarray_t    { jsarray };
    enum jsend_t      { jsend };
    enum jsescape_t   { jsescape };
    enum jsnoescape_t { jsnoescape };
    enum jsraw_t      { jsraw };
    enum jsnoraw_t    { jsnoraw };
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

//! specialize for types that are integral but not character-like
//! omitting signed char and unsigned char on purpose; these are usually arithmetic
template <class T> struct json_is_integral : std::is_integral<T> {};

template <> struct json_is_integral<char>     : std::false_type {};
template <> struct json_is_integral<char16_t> : std::false_type {};
template <> struct json_is_integral<char32_t> : std::false_type {};

//! json string escape
template <class Iterator>
inline void json_escape_string(std::ostream &o, Iterator iter, const Iterator stop) {
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

namespace impl {
    template <class T>
    inline constexpr T tens(int n) {
        return (n <= 0) ? (T)1 : (T)10 * tens<T>(n - 1);
    }
}


//! dangerously fast construction of json output, maybe
// XXX: you probably don't want to use this. see json.hh
struct jsonstream {
    static std::string double_to_string(double v);

    enum class state {
        none,
        object_begin,
        object_key,
        object_value,
        array_begin,
        array_value,
    };

    std::ostream &os;
    const std::ios_base::fmtflags os_oldflags;
    std::stack<state> states;
    bool escape = false;
    bool raw = false;

    jsonstream(std::ostream &os_) : os(os_), os_oldflags(os.flags()) {
        os << std::boolalpha;
        os << std::showpoint;
        states.emplace(state::none);
    }
    ~jsonstream() {
        os.flags(os_oldflags);
    }

    jsonstream &operator<<(const std::string &s) {
        transition(s, [=, &s] {
            if (raw) {
                os << s;
            } else {
                os << '"';
                if (escape) {
                    json_escape_string(os, begin(s), end(s));
                } else {
                    os << s;
                }
                os << '"';
            }
        });
        return *this;
    }

    jsonstream &operator<<(const char *s) {
        transition(s, [=] {
            if (raw) {
                os << s;
            } else {
                os << '"';
                if (escape) {
                    json_escape_string(os, s, s+strlen(s));
                } else {
                    os << s;
                }
                os << '"';
            }
        });
        return *this;
    }

    jsonstream &operator<<(bool b) {
        transition(b, [=] {
            os << b;
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        json_is_integral<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(number, [=] {
            using it = typename std::conditional<std::is_signed<T>::value, intmax_t, uintmax_t>::type;
            os << static_cast<it>(number);
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_floating_point<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(number, [=] {
            os << double_to_string(number);
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsraw_t) {
        raw = true;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsnoraw_t) {
        raw = false;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsescape_t) {
        escape = true;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsnoescape_t) {
        escape = false;
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsobject_t mip) {
        transition(mip, [=] {
            states.emplace(state::object_begin);
            os << '{';
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsarray_t mip) {
        transition(mip, [=] {
            states.emplace(state::array_begin);
            os << '[';
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::jsend_t) {
        switch (states.top()) {
            case state::object_begin:
                states.pop(); // empty object
                os << '}';
                break;
            case state::object_value:
                states.pop();
                if (states.top() != state::object_begin) {
                    throw jsonstream_error("bad state");
                }
                states.pop();
                os << '}';
                break;
            case state::array_begin:
                states.pop(); // empty array
                os << ']';
                break;
            case state::array_value:
                states.pop();
                if (states.top() != state::array_begin) {
                    throw jsonstream_error("bad state");
                }
                states.pop();
                os << ']';
                break;
            default:
                throw jsonstream_error("bad state");
        }
        return *this;
    }

    template <class T, class F>
    void transition(const T &, F &&f) {
        _transition(std::forward<F>(f), can_json_key<T>::value);
    }

    void _transition(std::function<void()> f, bool can_key) {
        switch (states.top()) {
            case state::none:
                f();
                break;
            case state::object_value:
                states.pop();
                os << ',';
                // FALLTHROUGH
            case state::object_begin:
                if (!can_key) {
                    throw jsonstream_error("invalid key");
                }
                states.emplace(state::object_key);
                f();
                os << ':';
                break;
            case state::object_key:
                states.top() = state::object_value;
                f();
                break;
            case state::array_begin:
                states.emplace(state::array_value);
                f();
                break;
            case state::array_value:
                os << ',';
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
