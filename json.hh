#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>
#include <string>
#include <ostream>
#include "error.hh"

namespace ten {

inline int ostream_json_dump_callback(const char *buffer, size_t size, void *data) {
    std::ostream *o = (std::ostream *)data;
    o->write(buffer, size);
    return 0;
}

inline std::ostream &operator <<(std::ostream &o, json_t *j) {
    if (!j) return o;
    json_dump_callback(j, ostream_json_dump_callback, &o, JSON_ENCODE_ANY);
    return o;
}

typedef std::shared_ptr<json_t> json_ptr;

class jsobj {
private:
    typedef void (jsobj::*unspecified_bool_type)() const;
    void true_value() const {}
    json_ptr p;
public:
    jsobj(json_t *j, bool incref=false) : p(j, json_decref) {
        if (incref) {
            json_incref(j);
        }
    }
    jsobj(const char *s, size_t flags=JSON_DECODE_ANY) {
        load(s, strlen(s), flags);
    }
    jsobj(const std::string &s, size_t flags=JSON_DECODE_ANY) {
        load(s.c_str(), s.size(), flags);
    }

    void load(const char *s, size_t len, size_t flags=JSON_DECODE_ANY) {
        json_error_t err;
        p.reset(json_loadb(s, len, flags, &err), json_decref);
        if (!p) {
            // TODO: custom exception
            throw errorx("%s", err.text);
        }
    }

    jsobj &operator = (json_t *j) {
        p.reset(j, json_decref);
        return *this;
    }

    bool operator == (const jsobj &rhs) const {
        return json_equal(p.get(), rhs.p.get());
    }

    bool operator == (json_t *rhs) {
        return json_equal(p.get(), rhs);
    }

    jsobj operator [](size_t index) {
        return jsobj(json_array_get(p.get(), index), true);
    }

    jsobj operator [](const std::string &key) {
        return jsobj(json_object_get(p.get(), key.c_str()), true);
    }

    std::string str() const {
        return json_string_value(p.get());
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
};
   
} // end namespace ten

#endif // JSON_HH
