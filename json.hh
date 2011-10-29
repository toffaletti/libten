#ifndef JSON_HH
#define JSON_HH

#include "jansson/jansson.h"
#include <memory>

namespace ten {

typedef std::shared_ptr<json_t> json_ptr;

} // end namespace ten

#endif // JSON_HH
