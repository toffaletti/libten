#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>

namespace fw {

struct free_deleter
{
    void operator() (void const *p) const {
        free((void *)p);
    }
};

struct json_deleter
{
    void operator() (void const *p) const {
        json_decref((json_t *)p);
    }
};

typedef std::shared_ptr<json_t> json_ptr;

} // end namespace fw

#endif // JSON_HH
