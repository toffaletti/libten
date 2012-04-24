#ifndef LIBTEN_JSERIAL_MAYBE_HH
#define LIBTEN_JSERIAL_MAYBE_HH

#include "ten/jserial.hh"
#include "ten/maybe.hh"
#include <type_traits>

namespace ten {
using std::move;

namespace detail {
    template <bool Save> struct serialize_maybe;
    template <> struct serialize_maybe<true> {
        template <class AR, class T>
        static void serialize(AR &ar, maybe<T> &m) {
            if (m.ok())
                ar << m.get_ref();
        }
    };
    template <> struct serialize_maybe<false> {
        template <class AR, class T>
        static void serialize(AR &ar, maybe<T> &m) {
            if (ar.source()) {
                T t;
                ar >> t;
                m = move(t);
            }
        }
    };
}

template <class AR, class T>
inline AR & operator & (AR &ar, maybe<T> &m) {
    detail::serialize_maybe<AR::is_save>::serialize(ar, m);
    return ar;
}

} // ten

#endif // LIBTEN_JSERIAL_MAYBE_HH
