#ifndef LIBTEN_JSERIAL_OPTIONAL_HH
#define LIBTEN_JSERIAL_OPTIONAL_HH

#include "ten/jserial.hh"
#include "ten/optional.hh"
#include <type_traits>

namespace ten {

namespace detail {

template <class AR, class T>
inline void serialize(AR &ar, optional<T> &m, std::true_type) {
    if (m)
        ar & *m;
}

template <class AR, class T>
inline void serialize(AR &ar, optional<T> &m, std::false_type) {
    if (!ar.empty()) {
        T t;
        ar & t;
        m = std::move(t);
    }
}
} // detail

// static
template <class AR, class T, class IsSave = typename AR::is_save>
inline void serialize(AR &ar, optional<T> &m) {
    detail::serialize(ar, m, IsSave());
}

// virtual
template <class T>
inline void serialize(json_archive &ar, optional<T> &m) {
    if (ar.is_save_v())
        detail::serialize(ar, m, std::true_type());
    else
        detail::serialize(ar, m, std::false_type());
}


} // ten

#endif // LIBTEN_JSERIAL_OPTIONAL_HH
