#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>
#include <string>
#include <ostream>
#include "error.hh"

namespace ten {

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
    jsobj(const char *s, size_t flags=0) : p((json_t *)0, json_decref) {
        json_error_t err;
        p.reset(json_loads(s, flags, &err));
        if (!p) {
            // TODO: custom exception
            throw errorx("%s", err.text);
        }
    }
    operator json_t *() {
        return p.get();
    }

    operator unspecified_bool_type() const {
        return p.get() != 0 ? &jsobj::true_value : 0;
    }


    jsobj path(const std::string &path);
};

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

    
} // end namespace ten

#endif // JSON_HH
