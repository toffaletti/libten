#ifndef LIBTEN_JSERIAL_HH
#define LIBTEN_JSERIAL_HH

#include "ten/json.hh"
#include "ten/error.hh"
#include <type_traits>

namespace ten {
using std::move;

// KV<> wraps a named field
// ConstKV<> wraps a named field that is not dynamic

template <class V> struct KV {
    const char *key;
    V &&value;
    KV(const char *k, V &&v) : key(k), value(v) {}
};
template <class V> struct ConstKV : public KV<V> {
    ConstKV(const char *k, V &&v) : KV<V>(k, v) {}
};

template <class V>      KV<V>       kv(const char *k, V &&v) { return      KV<V>(k, v); }
template <class V> ConstKV<V> const_kv(const char *k, V &&v) { return ConstKV<V>(k, v); }


// JSave

enum save_mode {
    save_all,
    save_dynamic
};

class JSave {
  private:
    json _j;
    unsigned _version;
    save_mode _mode;

  public:
    using is_archive = std::true_type;
    using is_save    = std::true_type;
    using is_load    = std::false_type;

    explicit JSave(unsigned ver = 0, save_mode mode = save_all)
        : _version(ver), _mode(mode) {}

    save_mode mode()   const { return _mode; }
    unsigned version() const { return _version; }

    // make one like the other but without memory
    JSave clone() const  { return JSave(_version, _mode); }

    // public value extraction
    friend json result(const JSave  &ar)  { return ar._j; }
    friend json result(      JSave &&ar)  { return move(ar._j); }

    // operator << works on save only,
    template <class T> JSave & operator << (T &t)           { return *this & t; }
    template <class V> JSave & operator << (     KV<V &> f) { return *this & f; }
    template <class V> JSave & operator << (ConstKV<V &> f) { return *this & f; }

    // save functions

    JSave & operator & (const json &j) {
        req_empty_for(j);
        _j = j;
        return *this;
    }

    template <class T, class X = typename std::enable_if<json_traits<T>::can_make>::type>
    JSave & operator & (const T &t) {
        json j(to_json(t));
        req_empty_for(j);
        _j = move(j);
        return *this;
    }

    // kv pair specialization

    template <class V>
    JSave & operator & (KV<V> f) {
        req_obj_for(f.key);
        auto js = clone();
        auto jv = result(js & f.value);
        if (jv)
            _j.set(f.key, move(jv));
        return *this;
    }

    template <class V>
    JSave & operator & (ConstKV<V> kv) {
        if (_mode == save_all)
            *this & static_cast<KV<V>&>(kv);
        return *this;
    }

  protected:
    // convenience for save functions
    void req_empty_for(const json &jn) {
        if (_j) throw errorx("JSave multiple: %s & %s", _j.dump().c_str(), jn.dump().c_str());
    }
    void req_obj_for(const char *key) {
        if (!_j) _j = json::object();
        else if (!_j.is_object()) throw errorx("JSave key '%s' to non-object: %s", key, _j.dump().c_str());
    }
};


// JLoad

class JLoad {
    json _j;
    unsigned _version;

  public:
    using is_archive = std::true_type;
    using is_save    = std::false_type;
    using is_load    = std::true_type;

    JLoad(const json & j, unsigned ver = 0) : _j(j), _version(ver) {}
    JLoad(      json &&j, unsigned ver = 0) : _j(j), _version(ver) {}

    json source()      const { return _j; }
    unsigned version() const { return _version; }

    // make one like the other but without memory
    JLoad clone() const  { return JLoad(_version); }

    // operator >> works on load only,
    template <class T> JLoad & operator >> (T &t)           { return *this & t; }
    template <class V> JLoad & operator >> (     KV<V &> f) { return *this & f; }  // not ref
    template <class V> JLoad & operator >> (ConstKV<V &> f) { return *this & f; }  // not ref

    // load functions

    JLoad & operator & (json &j) {
        j = _j;
        return *this;
    }

    template <class T, class X = typename std::enable_if<json_traits<T>::can_cast>::type>
    JLoad & operator & (T &t) {
        t = json_cast<T>(_j);
        return *this;
    }

    template <class V>
    JLoad & operator & (KV<V &> f) {
        req_obj_for(f.key);
        json jv(_j[f.key]);
        if (jv) {
            JLoad js(move(jv), _version);
            js & f.value;
        }
        return *this;
    }

  protected:
    // convenience for load functions
    void req_obj_for(const char *key) {
        if (!_j.is_object()) throw errorx("JLoad key '%s' from non-object %s", key, _j.dump().c_str());
    }
};

template <class T> inline json jsave_all(T &t) { JSave ar(save_all);     ar << t; return result(move(ar)); }
template <class T> inline json jsave_dyn(T &t) { JSave ar(save_dynamic); ar << t; return result(move(ar)); }

} // ten

#endif // LIBTEN_JSERIAL_HH
