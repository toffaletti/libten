#ifndef LIBTEN_JSERIAL_HH
#define LIBTEN_JSERIAL_HH

#include "ten/json.hh"
#include "ten/error.hh"
#include <type_traits>

// FIXME: in theory I shouldn't have to do this, but SafeInt<> defines
//   operator &.  revisit on rename to archive().
#include "ten/SafeInt3.hpp"

namespace ten {

// kvt<> wraps a named field
// const_kvt<> wraps a named field that is not dynamic

template <class V> struct kvt {
    const char *key;
    V &&value;
    kvt(const char *k, V &&v) : key(k), value(v) {}
};
template <class V> struct const_kvt : public kvt<V> {
    const_kvt(const char *k, V &&v) : kvt<V>(k, v) {}
};

template <class V> inline kvt<            V&>       kv(const char *k,       V &v) { return {k, v}; }
template <class V> inline kvt<      const V&>       kv(const char *k, const V &v) { return {k, v}; }
template <class V> inline const_kvt<      V&> const_kv(const char *k,       V &v) { return {k, v}; }
template <class V> inline const_kvt<const V&> const_kv(const char *k, const V &v) { return {k, v}; }


// json_archive, now with virtuality!

enum archive_mode {
    archive_dynamic,
    archive_all
};

class json_archive {
  protected:
    json _j;
    unsigned _version;
    archive_mode _mode;

    json_archive(unsigned ver = 0, archive_mode mode = archive_all) : _version(ver), _mode(mode) {}

  public:
    using is_archive = std::true_type;

    virtual bool is_save_v() const = 0;
    bool is_load_v() const { return !is_save_v(); }

    unsigned version() const   { return _version; }
    archive_mode mode() const   { return _mode; }

    bool empty() const  { return empty_v(); }
    virtual bool empty_v() const = 0;

    virtual void vserialize(json &j) = 0;
    virtual void vserialize(json &j, const char *key, archive_mode min_mode) = 0;

    json_archive & operator & (json &j)            { vserialize(j); return *this; }
    json_archive & operator & (      kvt<json&> f) { vserialize(f.value, f.key, archive_dynamic); return *this; }
    json_archive & operator & (const_kvt<json&> f) { vserialize(f.value, f.key, archive_all);     return *this; }
};


// json_saver

class json_saver : public json_archive {
  public:
    explicit json_saver(unsigned ver = 0, archive_mode mode = archive_all)
        : json_archive(ver, mode) {}

    using is_save    = std::true_type;
    using is_load    = std::false_type;

    bool empty() const  { return _j; }

    // public value extraction
    friend json result(const json_saver &ar)   { return ar._j; }
    friend json result(      json_saver &&ar)  { return move(ar._j); }

    // serialization, main interface
    friend void serialize (json_saver &ar, const json &j) {
        ar.req_empty_for(j);
        ar._j = j;
    }
    template <class T, class Enable = typename std::enable_if<json_traits<T>::can_make>::type>
    friend void serialize(json_saver &ar, const T &t) {
        serialize(ar, to_json(t));
    }

    // kvt<> and const_kvt<> specialization
    template <class V>
    friend void serialize(json_saver &ar, kvt<V&> f) {
        ar.req_obj_for(f.key);
        json_saver ar2(ar);
        serialize(ar2, f.value);
        auto j2(result(ar2));
        if (j2)
            ar._j.set(f.key, move(j2));
    }
    template <class V>
    friend void serialize(json_saver &ar, const_kvt<V&> f) {
        if (ar._mode > archive_dynamic)
            serialize(static_cast<kvt<V&> &>(f));
    }

    // SafeInt<> specialization - because SafeInt<> has operator &
    template <class I>
    friend void serialize(json_saver &ar, const SafeInt<I> &si) {
        serialize(ar, *si.Ptr());
    }

    // runtime polymorphic interface - premade json only
    bool is_save_v() const override {
        return true;
    }
    bool empty_v() const override {
        return empty();
    }
    void vserialize(json &j) override {
        serialize(*this, j);
    }
    void vserialize(json &j, const char *key, archive_mode min_mode) override {
        if (_mode >= min_mode)
            serialize(*this, kv(key, j));
    }

    // eye candy
    template <class T> json_saver & operator &  (T &t)            { serialize(*this, t); return *this; }
    template <class V> json_saver & operator &  (      kvt<V&> f) { serialize(*this, f); return *this; }
    template <class V> json_saver & operator &  (const_kvt<V&> f) { serialize(*this, f); return *this; }

    template <class T> json_saver & operator << (T &t)            { serialize(*this, t); return *this; }
    template <class V> json_saver & operator << (      kvt<V&> f) { serialize(*this, f); return *this; }
    template <class V> json_saver & operator << (const_kvt<V&> f) { serialize(*this, f); return *this; }

  protected:
    // convenience for save functions
    void req_empty_for(const json &jn) {
        if (_j) throw errorx("json_saver multiple: %s & %s", _j.dump().c_str(), jn.dump().c_str());
    }
    void req_obj_for(const char *key) {
        if (!_j) _j = json::object();
        else if (!_j.is_object()) throw errorx("json_saver key '%s' to non-object: %s", key, _j.dump().c_str());
    }
};


// json_loader

class json_loader : public json_archive {
  public:
    explicit json_loader(const json & j, unsigned ver = 0, archive_mode mode = archive_all)
        : json_archive(ver, mode) { _j = j; }
    explicit json_loader(      json &&j, unsigned ver = 0, archive_mode mode = archive_all)
        : json_archive(ver, mode) { _j = j; }

    using is_save    = std::false_type;
    using is_load    = std::true_type;

    bool empty() const  { return _j; }

    // serialization, main interface
    friend void serialize (json_loader &ar, json &j) {
        j = ar._j;
    }
    template <class T, class Enable = typename std::enable_if<json_traits<T>::can_cast>::type>
    friend void serialize(json_loader &ar, T &t) {
        t = json_cast<T>(ar._j);
    }

    // kvt<> and const_kvt<> specialization
    template <class V>
    friend void serialize(json_loader &ar, kvt<V&> f) {
        ar.req_obj_for(f.key);
        json jv(ar._j[f.key]);
        if (jv) {
            json_loader ar2(std::move(jv), ar._version, ar._mode);
            serialize(ar2, f.value);
        }
    }
    template <class V>
    friend void serialize(json_loader &ar, const_kvt<V> f) {
        if (ar._mode > archive_dynamic)
            serialize(ar, static_cast<kvt<V> &>(f));
    }

    // SafeInt<> specialization - because SafeInt<> has operator &
    template <class I>
    friend void serialize(json_loader &ar, SafeInt<I> &si) {
        serialize(ar, *si.Ptr());
    }

    // runtime polymorphic interface - premade json only
    bool is_save_v() const override {
        return false;
    }
    bool empty_v() const override {
        return empty();
    }
    void vserialize(json &j) override {
        serialize(*this, j);
    }
    void vserialize(json &j, const char *key, archive_mode min_mode) override {
        if (_mode >= min_mode)
            serialize(*this, kv(key, j));
    }

    // eye candy
    template <class T> json_loader & operator &  (T &t)            { serialize(*this, t); return *this; }
    template <class V> json_loader & operator &  (      kvt<V&> f) { serialize(*this, f); return *this; }
    template <class V> json_loader & operator &  (const_kvt<V&> f) { serialize(*this, f); return *this; }

    template <class T> json_loader & operator >> (T &t)            { serialize(*this, t); return *this; }
    template <class V> json_loader & operator >> (      kvt<V&> f) { serialize(*this, f); return *this; }
    template <class V> json_loader & operator >> (const_kvt<V&> f) { serialize(*this, f); return *this; }

  protected:
    // convenience for load functions
    void req_obj_for(const char *key) {
        if (!_j.is_object()) throw errorx("json_loader key '%s' from non-object %s", key, _j.dump().c_str());
    }
};


// global functions

template <class T> inline json jsave_all(const T &t) { json_saver ar(0, archive_all);     ar << const_cast<T&>(t); return result(std::move(ar)); }
template <class T> inline json jsave_dyn(const T &t) { json_saver ar(0, archive_dynamic); ar << const_cast<T&>(t); return result(std::move(ar)); }

} // ten

#endif // LIBTEN_JSERIAL_HH
