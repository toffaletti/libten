#ifndef TEN_OPTIONAL_HH
#define TEN_OPTIONAL_HH

#include "ten/bits/optional.hpp"

namespace ten {
    using std::experimental::optional;
    using std::experimental::nullopt;
    using std::experimental::emplace;
    using std::experimental::emplace_t;
    using std::experimental::is_not_optional;
    using std::experimental::get_value_or;
}

#endif
