#include "msgpack/msgpack.hpp"
#include "json.hh"

namespace msgpack {

using ten::json;

inline json& operator>> (object o, json& v)
{
    switch (o.type) {
        case type::NIL:
            v = json::null();
            return v;
        case type::BOOLEAN:
            if (o.via.boolean) {
                v = json::jtrue();
            } else {
                v = json::jfalse();
            }
            return v;
        case type::POSITIVE_INTEGER:
        case type::NEGATIVE_INTEGER:
		    v = json::integer((json_int_t)o.via.i64);
            return v;
        case type::DOUBLE:
            v = json::real(o.via.dec);
            return v;
        case type::RAW:
            {
                std::string s(o.via.raw.ptr, o.via.raw.size);
                v = json::str(s);
            }
            return v;
        case type::ARRAY:
            {
                v = json::array();
                for(object* p(o.via.array.ptr),
                        * const pend(o.via.array.ptr + o.via.array.size);
                        p < pend; ++p) {
                    v.push(p->as<json>());
                }
            }
            return v;
        case type::MAP:
            {
                v = json::object();
                for(object_kv* p(o.via.map.ptr),
                        * const pend(o.via.map.ptr + o.via.map.size);
                        p < pend; ++p) {
                    std::string key(p->key.via.raw.ptr, p->key.via.raw.size);
                    v.set(key, p->val.as<json>());
                }
            }
            return v;
        default:
            throw type_error();
    }
}



template <typename Stream>
packer<Stream>& operator<< (packer<Stream>& o, const json& v)
{
    switch(v.type()) {
        case JSON_NULL:
            o.pack_nil();
            return o;

        case JSON_TRUE:
            o.pack_true();
            return o;

        case JSON_FALSE:
            o.pack_false();
            return o;

        case JSON_INTEGER:
            if (v.integer() >= 0) {
                o.pack_uint64(v.integer());
            } else {
                o.pack_int64(v.integer());
            }
            return o;

        case JSON_REAL:
            o.pack_double(v.real());
            return o;

        case JSON_STRING:
            {
                std::string s = v.str();
                o.pack_raw(s.size());
                o.pack_raw_body(s.data(), s.size());
            }
            return o;

        case JSON_ARRAY:
            {
                json vv(v);
                o.pack_array(vv.asize());
                for (auto i=vv.abegin(); i!=vv.aend(); ++i) {
                    o << *i;
                }
            }
            return o;

        case JSON_OBJECT:
            {
                json vv(v);
                o.pack_map(vv.osize());
                for (auto i=vv.obegin(); i!=vv.oend(); ++i) {
                    std::string key((*i).first);
                    o << key << (*i).second;
                }
            }
            return o;

        default:
            throw type_error();
    }
}

} // end namespace msgpack
