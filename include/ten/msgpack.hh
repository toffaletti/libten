#ifndef LIBTEN_MSGPACK_HH
#define LIBTEN_MSGPACK_HH

#include "msgpack/msgpack.hpp"
#include "ten/optional.hh"

namespace msgpack {

template <typename T>
inline ten::optional<T> & operator>> (object o, ten::optional<T>& v) {
    switch (o.type) {
        case type::NIL:
            v = ten::nullopt;
            return v;
        default:
            {
                T t;
                o >> t;
                v = t;
                return v;
            }
    }
}

template <typename Stream, typename T>
packer<Stream>& operator<< (packer<Stream>& o, const ten::optional<T>& v) {
    if (v) {
        o << *v;
    } else {
        o.pack_nil();
    }
    return o;
}

} // end msgpack

#endif
