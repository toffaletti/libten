#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>
#include <string>
#include <ostream>
#include <sstream>
#include <functional>
#include "error.hh"

namespace ten {

enum json_take_t { json_take };

template <class J> class shared_json_ptr {
private:
    typedef void (shared_json_ptr::*unspecified_bool_type)() const;
    void true_value() const {}
    J *p;
public:
    // ctor, dtor, assign
    shared_json_ptr()                          : p() {}
    explicit shared_json_ptr(J *j)             : p(json_incref(j)) {}
    shared_json_ptr(J *j, json_take_t)         : p(j) {}
    ~shared_json_ptr()                         { json_decref(p); }

    // construction and assignment - make the compiler work for a living
    template <class J2> shared_json_ptr(const shared_json_ptr<J2>  &jp) : p(json_incref(jp.get())) {}
    template <class J2> shared_json_ptr(      shared_json_ptr<J2> &&jp) : p(jp.release())          {}
    template <class J2> shared_json_ptr & operator = (const shared_json_ptr<J2>  &jp) { reset(jp.ptr());    return *this; }
    template <class J2> shared_json_ptr & operator = (      shared_json_ptr<J2> &&jp) { take(jp.release()); return *this; }

    // object interface

    void reset(J *j) { take(json_incref(j)); }
    void take(J *j)  { json_decref(p); p = j; }  // must be convenient for use with C code

    J * operator -> () const { return p; }
    J *get() const           { return p; }
    J *release()             { J *j = p; p = nullptr; return j; }

    operator unspecified_bool_type () const {
        return p ? &shared_json_ptr::true_value : nullptr;
    }
};

typedef shared_json_ptr<      json_t>       json_ptr;
typedef shared_json_ptr<const json_t> const_json_ptr;

template <class J> shared_json_ptr<J> make_json_ptr(J *j) { return shared_json_ptr<J>(j); }
template <class J> shared_json_ptr<J> take_json_ptr(J *j) { return shared_json_ptr<J>(j, json_take); }

std::ostream & operator << (std::ostream &o, const json_t *j);
inline std::ostream & operator << (std::ostream &o,       json_ptr jp) { return o << jp.get(); }
inline std::ostream & operator << (std::ostream &o, const_json_ptr jp) { return o << jp.get(); }


class jsobj {
private:
    typedef void (jsobj::*unspecified_bool_type)() const;
    void true_value() const {}
    json_ptr p;
public:
    jsobj(json_t *j) : p(j) {}
    jsobj(const char *s, size_t flags=JSON_DECODE_ANY) {
        load(s, strlen(s), flags);
    }
    jsobj(const std::string &s, size_t flags=JSON_DECODE_ANY) {
        load(s.c_str(), s.size(), flags);
    }

    void load(const char *s, size_t len, size_t flags=JSON_DECODE_ANY) {
        json_error_t err;
        p.reset(json_loadb(s, len, flags, &err));
        if (!p) {
            // TODO: custom exception
            throw errorx("%s", err.text);
        }
    }

    std::string dump(size_t flags=JSON_ENCODE_ANY) {
        std::stringstream ss;
        ss << p.get();
        return ss.str();
    }

    jsobj &operator = (json_t *j) {
        p.reset(j);
        return *this;
    }
    jsobj &operator = (const json_ptr &jp) {
        p = jp;
        return *this;
    }

    bool operator == (const jsobj &rhs) const {
        return json_equal(p.get(), rhs.p.get());
    }

    bool operator == (json_t *rhs) {
        return json_equal(p.get(), rhs);
    }

    jsobj operator [](size_t index) {
        return jsobj(json_incref(json_array_get(p.get(), index)));
    }

    jsobj operator [](const std::string &key) {
        return jsobj(json_incref(json_object_get(p.get(), key.c_str())));
    }

    std::string str() const {
        return json_string_value(p.get());
    }

    void set(const std::string &s) {
        if (json_string_set(p.get(), s.c_str()) != 0) {
            throw errorx("error setting string");
        }
    }

    void set(const std::string &k, const std::string &v) {
        if (json_object_set_new_nocheck(p.get(), k.c_str(), json_string(v.c_str())) != 0) {
            throw errorx("error setting key to value");
        }
    }

    size_t size() const {
        if (json_is_object(p.get())) {
            return json_object_size(p.get());
        } else if (json_is_array(p.get())) {
            return json_array_size(p.get());
        }
        throw errorx("size not valid for type");
    }

    json_t *ptr() const {
        return p.get();
    }

    friend std::ostream &operator <<(std::ostream &o, const jsobj &j) {
        o << j.p.get();
        return o;
    }

    operator unspecified_bool_type() const {
        return p.get() != 0 ? &jsobj::true_value : 0;
    }

    jsobj path(const std::string &path);

    typedef std::function<bool (json_t *parent, const char *key, json_t *value)> visitor_func_t;
    void visit(const visitor_func_t &visitor);
};
   
} // end namespace ten

#endif // JSON_HH
