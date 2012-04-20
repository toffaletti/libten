#ifndef LIBTEN_JSERIAL_MAYBE_HH
#define LIBTEN_JSERIAL_MAYBE_HH

#include "ten/jserial.hh"
#include "ten/maybe.hh"
#include <type_traits>

namespace ten {
using std::move;

template <class AR, class T, class X = typename std::enable_if<AR::is_save>::type>
inline AR & operator >> (AR &ar, maybe<T> &m) {
    if (_j) {
        T t;
        ar >> t;
        m = move(t);
    }
    return ar;
}

template <class AR, class T, class X = typename std::enable_if<AR::is_load>::type>>
inline AR & operator << (AR &ar, maybe<T> &m) {
    if (m.ok())
        ar << m.get_ref();
    return ar;
}

} // ten

#endif LIBTEN_JSERIAL_MAYBE_HH
