#ifndef LIBTEN_MAYBEPACK_HH
#define LIBTEN_MAYBEPACK_HH

#include "msgpack/msgpack.hpp"
#include "ten/maybe.hh"

namespace msgpack {

template <typename T>
inline ten::maybe<T> & operator>> (object o, ten::maybe<T>& v) {
    switch (o.type) {
        case type::NIL:
            v.reset();
            return v;
        default:
            {
                T t;
                o >> t;
                v.reset(t);
                return v;
            }
    }
}

template <typename Stream, typename T>
packer<Stream>& operator<< (packer<Stream>& o, const ten::maybe<T>& v) {
    if (v.ok()) {
        o << v.get_ref();
    } else {
        o.pack_nil();
    }
    return o;
}

} // end msgpack

#endif
