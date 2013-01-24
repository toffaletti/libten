#ifndef JSONSTREAM_HH
#define JSONSTREAM_HH

#include <iomanip>
#include <sstream>
#include <stack>
#include <type_traits>

namespace jsonstream_manip {
    namespace detail {
        struct begin_object {};
        struct end_object {};
        struct begin_array {};
        struct end_array {};
    }

    detail::begin_object begin_object;
    detail::end_object end_object;
    detail::begin_array begin_array;
    detail::end_array end_array;
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

    mutable std::ostringstream ss;
    std::stack<state> states;

    jsonstream() {
        ss << std::boolalpha;
        ss << std::fixed;
        ss << std::showpoint;
        states.emplace(state::none);
    }

    std::string str() {
        return ss.str();
    }

    template <class Func>
    void transition(bool can_key, Func &&f) {
        switch (states.top()) {
            case state::none:
                f();
                break;
            case state::object_begin:
                if (!can_key) throw std::runtime_error("cant be a key: " + ss.str());
                states.emplace(state::object_key);
                f();
                ss << ":";
                break;
            case state::object_key:
                states.top() = state::object_value;
                f();
                break;
            case state::object_value:
                if (!can_key) throw std::runtime_error("cant be a key: " + ss.str());
                states.top() = state::object_key;
                ss << ",";
                f();
                ss << ":";
                break;
            case state::array_begin:
                states.emplace(state::array_value);
                f();
                break;
            case state::array_value:
                ss << ",";
                f();
                break;
        }
    }

    jsonstream &operator<<(const char *s) {
        transition(true, [&] {
            ss << "\"" << s << "\"";
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_floating_point<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(false, [&] {
            //ss << std::setprecision(std::numeric_limits<T>::digits10+1);
            ss << number;
        });
        return *this;
    }

    template <class T>
    typename std::enable_if<
        std::is_integral<T>::value, jsonstream &>::type
    operator<<(T number) {
        transition(false, [&] {
            ss << number;
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::begin_object mip) {
        transition(false, [&] {
            states.emplace(state::object_begin);
            ss << "{";
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::end_object mip) {
        switch (states.top()) {
            case state::object_begin:
                states.pop(); // empty object
                break;
            case state::object_value:
                states.pop();
                CHECK(states.top() == state::object_begin);
                states.pop();
                break;
            default:
                throw std::runtime_error("bad state" + ss.str());
        }
        ss << "}";
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::begin_array mip) {
        transition(false, [&] {
            states.emplace(state::array_begin);
            ss << "[";
        });
        return *this;
    }

    jsonstream &operator<<(jsonstream_manip::detail::end_array mip) {
        switch (states.top()) {
            case state::array_begin:
                states.pop(); // empty array
                break;
            case state::array_value:
                states.pop();
                CHECK(states.top() == state::array_begin);
                states.pop();
                break;
            default:
                throw std::runtime_error("bad state " + ss.str());
        }
        ss << "]";
        return *this;
    }

};

#endif
