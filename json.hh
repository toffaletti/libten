#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>

namespace fw {

typedef std::shared_ptr<json_t> json_ptr;

} // end namespace fw

#endif // JSON_HH
