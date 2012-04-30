#ifndef LIBTEN_JSERIAL_SAFEINT_HH
#define LIBTEN_JSERIAL_SAFEINT_HH

#include "ten/jserial.hh"
#include "ten/SafeInt3.hpp"
#include <type_traits>

namespace ten {
using std::move;

template <class I>
struct json_traits<SafeInt<I>> : public json_traits_conv<SafeInt<I>> {
    static SafeInt<I> cast(const json &j)      { return json_traits<I>::cast(j); }
    static json make(SafeInt<I> i)             { return json_traits<I>::make(i); }
};

#if 0 // This has moved to jserial due to SafeInt<> having an operator &
template <class AR, class I, class X = typename std::enable_if<AR::is_archive>::type>
inline AR & operator & (AR &ar, SafeInt<I> &si) {
    ar & *si.Ptr();
    return ar;
}
#endif

} // ten

#endif // LIBTEN_JSERIAL_SAFEINT_HH
