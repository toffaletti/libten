#ifndef TEN_JSONSTREAM_HH
#define TEN_JSONSTREAM_HH

#include <iomanip>
#include <stdexcept>
#include <stack>
#include <type_traits>
#include <cmath>

namespace ten {

namespace jsonstream_manip {
    namespace detail {
        struct jsobject {};
        struct jsarray {};
        struct jsend {};
    }

    detail::jsobject jsobject;
    detail::jsarray jsarray;
    detail::jsend jsend;
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

    jsonstream(std::ostream &os_) : os (os_) {
        os << std::boolalpha;
        os << std::showpoint;
        states.emplace(state::none);
    }

    jsonstream &operator<<(const std::string &s) {
        transition(s, [&] {
            os << "\"" << s << "\"";
        });
        return *this;
    }

    jsonstream &operator<<(const char *s) {
        transition(s, [&] {
            os << "\"" << s << "\"";
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
                //os << std::setprecision(std::numeric_limits<T>::digits10+1);
                os << number;
            }
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_integral<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(number, [&] {
            os << number;
        });
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

private:
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
