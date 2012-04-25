#ifndef LIBTEN_JSERIAL_ENUM_HH
#define LIBTEN_JSERIAL_ENUM_HH

#include "ten/jserial.hh"
#include <algorithm>
#include <type_traits>

namespace ten {
using std::string;

// first define an std::array<const char *, N> of enum names
// then call this as the body of your enum's operator &

namespace detail {
template <class AR, class E, class Iter>
inline AR & serialize_enum(AR &ar, E &e, Iter names_start, Iter names_end, std::true_type) {
    return ar & json::str(*(names_start + (ptrdiff_t)e));
}
template <class AR, class E, class Iter>
inline AR & serialize_enum(AR &ar, E &e, Iter names_start, Iter names_end, std::false_type) {
    json j;
    ar & j;
    if (j.is_string()) {
        auto nit = std::find(names_start, names_end, j.str());
        if (nit != names_end) {
            e = E(nit - names_start);
            return ar;
        }
    }
    throw errorx("invalid %s: %s", typeid(E).name(), j.dump().c_str());
}
} // detail

template <class AR, class E, class Iter>
inline AR & serialize_enum(AR &ar, E &e, Iter names_start, Iter names_end) {
    return detail::serialize_enum(ar, e, names_start, names_end, typename AR::is_save());
}
template <class AR, class E, class Coll>
inline AR & serialize_enum(AR &ar, E &e, const Coll &c) {
    return detail::serialize_enum(ar, e, c.begin(), c.end(), typename AR::is_save());
}

} // ten

#endif // LIBTEN_JSERIAL_MAYBE_HH
