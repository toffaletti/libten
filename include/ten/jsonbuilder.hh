#ifndef JSONBUILDER_HH
#define JSONBUILDER_HH

#include <sstream>

//! dangerously fast construction of json output, maybe
// XXX: you probably don't want to use this. see json.hh
class jsonbuilder {
private:
    std::ostringstream ss;
public:
    jsonbuilder() {
        ss.precision(17);
    }

    std::string build() {
        std::string tmp = ss.str();
        ss.str("");
        return tmp;
    }

    template <class ...Args>
        void object(Args ...args) {
            static_assert(sizeof...(args) % 2 == 0, "mismatched key/value pair");
            ss << "{";
            _object(args...);
            ss << "}";
        }

    template <class ...Args>
        void array(Args ...args) {
            ss << "[";
            _array(args...);
            ss << "]";
        }

private:
    void _key(const char *s) {
        _value(s);
    }

    void _key(const std::string &s) {
        _value(s);
    }

    void _value(const std::string &s) {
        ss << "\"" << s << "\"";
    }

    void _value(const char *s) {
        ss << "\"" << s << "\"";
    }

    void _value(const int i) {
        ss << i;
    }

    void _value(const unsigned i) {
        ss << i;
    }

    void _value(const float f) {
        ss << f;
    }

    void _object() {}

    template <class Key, class Value>
        void _object(const Key &k, const Value &v) {
            _key(k);
            ss << ":";
            _value(v);
        }

    template <class Key, class Value, class ...Args>
        void _object(const Key &k, const Value &v, Args ...args) {
            _key(k);
            ss << ":";
            _value(v);
            ss << ",";
            _object(args...);
        }

    void _array() {}

    template <class Value>
        void _array(const Value &v) {
            _value(v);
        }

    template <class Value, class ...Args>
        void _array(const Value &v, Args ...args) {
            _value(v);
            ss << ",";
            _array(args...);
        }
};

#endif
