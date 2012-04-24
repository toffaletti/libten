#ifndef LIBTEN_JSERIAL_SAFEINT_HH
#define LIBTEN_JSERIAL_SAFEINT_HH

#include "ten/jserial.hh"
#include "ten/SafeInt3.hpp"
#include <type_traits>

namespace ten {
using std::move;

template <class I>
struct json_traits<SafeInt<I>> : public json_traits_conv<SafeInt<I>> {
    static SafeInt<I> cast(const json &j)          { return json_cast<I>(j); }
};
template <class I>
inline json to_json(SafeInt<I> i)             { return to_json(i.Ref()); }


template <class AR, class I, class X = typename std::enable_if<AR::is_archive>::type>
inline AR & operator & (AR &ar, SafeInt<I> &si) {
    ar & *si.Ptr();
    return ar;
}

} // ten

#endif // LIBTEN_JSERIAL_SAFEINT_HH
