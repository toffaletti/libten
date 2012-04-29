#ifndef LIBTEN_JSERIAL_MAYBE_HH
#define LIBTEN_JSERIAL_MAYBE_HH

#include "ten/jserial.hh"
#include "ten/maybe.hh"
#include <type_traits>

namespace ten {

namespace detail {

template <class AR, class T>
inline void serialize(AR &ar, maybe<T> &m, std::true_type) {
    if (m.ok())
        ar << m.get_ref();
}

template <class AR, class T>
inline void serialize(AR &ar, maybe<T> &m, std::false_type) {
    if (ar.source()) {
        T t;
        ar >> t;
        m = std::move(t);
    }
}
} // detail

template <class AR, class T>
inline AR & operator & (AR &ar, maybe<T> &m) {
    detail::serialize(ar, m, typename AR::is_save());
    return ar;
}

} // ten

#endif // LIBTEN_JSERIAL_MAYBE_HH
