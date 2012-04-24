#ifndef LIBTEN_JSERIAL_ENUM_HH
#define LIBTEN_JSERIAL_ENUM_HH

#include "ten/jserial.hh"
#include <array>

namespace ten {
using std::string;

// first define an std::array<const char *, N> of enum names
// then call this as the body of your enum's operator &

template <class AR, class E, size_t NameCount>
inline AR & serialize_enum(AR &ar, E &e, const std::array<const char *, NameCount> &names) {
    if (AR::is_save) {
        // this is a kludge for unified archiving.  constness is a PITA
        json j(names[(size_t)e]);
        return ar & j;
    }
    else {
        json j;
        ar & j;
        if (j.is_string()) {
            for (size_t i = 0; i < NameCount; ++i) {
                if (!strcmp(names[i], j.c_str())) {
                    e = E(i);
                    return ar;
                }
            }
        }
        throw errorx("invalid %s: %s", typeid(E).name(), j.dump().c_str());
    }
}

} // ten

#endif // LIBTEN_JSERIAL_MAYBE_HH
