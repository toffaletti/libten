#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>
#include <ostream>

namespace ten {

typedef std::shared_ptr<json_t> json_ptr;

inline int ostream_json_dump_callback(const char *buffer, size_t size, void *data) {
    std::ostream *o = (std::ostream *)data;
    o->write(buffer, size);
    return 0;
}

inline std::ostream &operator <<(std::ostream &o, json_t *j) {
    json_dump_callback(j, ostream_json_dump_callback, &o, 0);
    return o;
}

} // end namespace ten

#endif // JSON_HH
