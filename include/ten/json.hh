#ifndef LIBTEN_JSON_HH
#define LIBTEN_JSON_HH

#include <jansson.h>
#include <string.h>
#include <string>
#include <functional>
#include <type_traits>

#include <limits.h>
#ifndef JSON_INTEGER_IS_LONG_LONG
# error Y2038
#endif

namespace ten {

//----------------------------------------------------------------
// streams meet json_t (just barely)
//

void dump(std::ostream &o, const json_t *j, unsigned flags = JSON_ENCODE_ANY);


//----------------------------------------------------------------
// json_ptr ... like shared_ptr but only for Jansson's json_t
// not generally for application use; prefer 'json' type
//

enum json_take_t { json_take };

namespace impl {

class json_ptr {
private:
    json_t *p;

public:
    // ctor, dtor, assign
    json_ptr()                          : p() {}
    explicit json_ptr(json_t *j)        : p(json_incref(j)) {}
    json_ptr(json_t *j, json_take_t)    : p(j) {}
    ~json_ptr()                         { json_decref(p); }

    // copy, move, assign, and move-assign
    json_ptr(              const json_ptr  &jp) : p(json_incref(jp.get())) {}
    json_ptr(                    json_ptr &&jp) : p(jp.release())          {}
    json_ptr & operator = (const json_ptr  &jp) { reset(jp.get());    return *this; }
    json_ptr & operator = (      json_ptr &&jp) { take(jp.release()); return *this; }

    // boolean truth === has json_t of any type
    explicit operator bool () const { return get(); }

    // object interface
    void reset(json_t *j)  { take(json_incref(j)); }
    void take(json_t *j)   { json_decref(p); p = j; }  // must be convenient for use with C code

    // smart pointer interface
    json_t * operator -> () const { return p; }
    json_t *get() const           { return p; }
    json_t *release()             { auto j = p; p = nullptr; return j; }

    // "show me"
    friend std::ostream & operator << (std::ostream &o, const json_ptr &jp) {
        dump(o, jp.get());
        return o;
    }
};

} // impl


//----------------------------------------------------------------
// type-safe convenient json_t access
//

// we prefer this enum for C++ because we want JSON_INVALID
// otherwise we'd use json_type.   maybe should change jansson
enum ten_json_type {
    TEN_JSON_OBJECT  = ::JSON_OBJECT,
    TEN_JSON_ARRAY   = ::JSON_ARRAY,
    TEN_JSON_STRING  = ::JSON_STRING,
    TEN_JSON_INTEGER = ::JSON_INTEGER,
    TEN_JSON_REAL    = ::JSON_REAL,
    TEN_JSON_TRUE    = ::JSON_TRUE,
    TEN_JSON_FALSE   = ::JSON_FALSE,
    TEN_JSON_NULL    = ::JSON_NULL,
    JSON_INVALID     = -1
};

//! represent JSON values
class json {
private:
    using json_ptr = impl::json_ptr;
    json_ptr _p;

    // autovivify array or object
    json_t *_o_get() {
        if (!_p)
            _p.take(json_object());
        return get();
    }
    json_t *_a_get() {
        if (!_p)
            _p.take(json_array());
        return get();
    }
    // helper versions that depend on the key for tagging
    json_t *_ao_get(const char *)         { return _o_get(); }
    json_t *_ao_get(const std::string &)  { return _o_get(); }
    json_t *_ao_get(int)                  { return _a_get(); }
    json_t *_ao_get(size_t)               { return _a_get(); }

public:
    // construction and assignment,
    //   including a selection of conversions from basic types

    json()                                     : _p()                 {}
    json(const json  &js)                      : _p(js._p)            {}
    json(      json &&js)                      : _p(std::move(js._p)) {}
    explicit json(json_t *j)                   : _p(j)                {}
             json(json_t *j, json_take_t t)    : _p(j, t)             {}
    json & operator = (const json  &js)        { _p = js._p;            return *this; }
    json & operator = (      json &&js)        { _p = std::move(js._p); return *this; }

    json(const char *s)                        : _p(json_string(s),                 json_take) {}
    json(const std::string &s)                 : _p(json_string(s.c_str()),         json_take) {}
    json(int i)                                : _p(json_integer(i),                json_take) {}
    json(long i)                               : _p(json_integer(i),                json_take) {}
    json(long long i)                          : _p(json_integer(i),                json_take) {}
    json(unsigned u)                           : _p(json_integer(u),                json_take) {}
#if ULONG_MAX < LLONG_MAX
    json(unsigned long u)                      : _p(json_integer(u),                json_take) {}
#endif
    json(double r)                             : _p(json_real(r),                   json_take) {}
    json(bool b)                               : _p(b ? json_true() : json_false(), json_take) {}

    static json object()                   { return json(json_object(),   json_take); }
    static json array()                    { return json(json_array(),    json_take); }
    static json str(const char *s)         { return json(s); }
    static json str(const std::string &s)  { return json(s); }
    static json integer(int i)             { return json(i); }
    static json integer(long i)            { return json(i); }
    static json integer(long long i)       { return json(i); }
    static json integer(unsigned u)        { return json(u); }
#if ULONG_MAX < LLONG_MAX
    static json integer(unsigned long u)   { return json(u); }
#endif
    static json real(double d)             { return json(d); }
    static json boolean(bool b)            { return json(b); }
    static json jtrue()                    { return json(json_true(),     json_take); }
    static json jfalse()                   { return json(json_false(),    json_take); }
    static json null()                     { return json(json_null(),     json_take); }

    // default to building objects, they're more common
    json(std::initializer_list<std::pair<const char *, json>> init)
        : _p(json_object(), json_take)
    {
        for (const auto &kv : init)
            set(kv.first, kv.second);
    }
    static json array(std::initializer_list<json> init) {
        json a = array();
        for (const auto &j : init)
            a.push(j);
        return a;
    }

    // copying

    json copy()      const                     { return json(json_copy(get()),      json_take); }
    json deep_copy() const                     { return json(json_deep_copy(get()), json_take); }

    // manipulation via json_t*

    friend json take_json(json_t *j)           { return json(j, json_take); }
    void take(json_t *j)                       { _p.take(j); }
    void reset(json_t *j)                      { _p.reset(j); }
    json_t *get() const                        { return _p.get(); }
    json_t *release()                          { return _p.release(); }

    // operator interface

    // boolean truth === has json_t of any type
    explicit operator bool () const            { return get(); }

    friend bool operator == (const json &lhs, const json &rhs) {
        // jansson doesn't think null pointers are equal
        const auto jl = lhs.get();
        const auto jr = rhs.get();
        return (!jl && !jr) || json_equal(jl, jr);
    }
    friend bool operator != (const json &lhs, const json &rhs) {
        return !(lhs == rhs);
    }

    // parse and output

    static json load(const std::string &s, unsigned flags = JSON_DECODE_ANY)  { return load(s.data(), s.size(), flags); }
    static json load(const char *s, unsigned flags = JSON_DECODE_ANY)    { return load(s, strlen(s), flags); }
    static json load(const char *s, size_t len, unsigned flags);

    std::string dump(unsigned flags = JSON_ENCODE_ANY) const;

    friend std::ostream & operator << (std::ostream &o, const json &j) {
        ten::dump(o, j.get());
        return o;
    }

    // type access

    ten_json_type type() const  { json_t *j = get(); return j ? (ten_json_type)json_typeof(j) : JSON_INVALID; }
    bool is_object()     const  { return json_is_object(get()); }
    bool is_array()      const  { return json_is_array(get()); }
    bool is_aggregate()  const  { return is_array() || is_object(); }
    bool is_string()     const  { return json_is_string(get()); }
    bool is_integer()    const  { return json_is_integer(get()); }
    bool is_real()       const  { return json_is_real(get()); }
    bool is_number()     const  { return json_is_number(get()); }
    bool is_true()       const  { return json_is_true(get()); }
    bool is_false()      const  { return json_is_false(get()); }
    bool is_boolean()    const  { return json_is_boolean(get()); }
    bool is_null()       const  { return json_is_null(get()); }

    // scalar access

    std::string str()    const  { return json_string_value(get()); }
    const char *c_str()  const  { return json_string_value(get()); }
    json_int_t integer() const  { return json_integer_value(get()); }
    double real()        const  { return json_real_value(get()); }
    bool boolean()       const  { return json_is_true(get()); }

    // 'truthiness' a la JavaScript - useful for config files etc.

    bool truthy() const  {
        switch (type()) {
        case TEN_JSON_FALSE:   return false;
        case TEN_JSON_STRING:  return *c_str();
        case TEN_JSON_INTEGER: return integer();
        case TEN_JSON_REAL:    return real();
        default:               return false;
        }
    }

    // scalar mutation - TODO - is this worth keeping?

    bool set_string(const char *sv)         { return !json_string_set( get(), sv); }
    bool set_string(const std::string &sv)  { return !json_string_set( get(), sv.c_str()); }
    bool set_integer(json_int_t iv)         { return !json_integer_set(get(), iv); }
    bool set_real(double rv)                { return !json_real_set(   get(), rv); }

    // aggregate access

    template <class Key>
    json operator [] (Key &&key)            { return get(std::forward<Key>(key)); }

    size_t osize() const                               { return      json_object_size(   get());                       }
    json get(   const char *key)                       { return json(json_object_get(    get(), key));                 }
    json get(   const std::string &key)                { return json(json_object_get(    get(), key.c_str()));         }
    bool set(   const char *key,        const json &j) { return     !json_object_set( _o_get(), key,         j.get()); }
    bool set(   const std::string &key, const json &j) { return     !json_object_set( _o_get(), key.c_str(), j.get()); }
    bool erase( const char *key)                       { return     !json_object_del(    get(), key);                  }
    bool erase( const std::string &key)                { return     !json_object_del(    get(), key.c_str());          }
    bool oclear()                                      { return     !json_object_clear(  get());                       }
    bool omerge(         const json &oj)               { return     !json_object_update(          get(), oj.get());    }
    bool omerge_existing(const json &oj)               { return     !json_object_update_existing( get(), oj.get());    }
    bool omerge_missing( const json &oj)               { return     !json_object_update_missing(  get(), oj.get());    }

    size_t asize() const                               { return      json_array_size(   get());              }
    json get(            int i)                        { return json(json_array_get(    get(), i));          }
    json get(         size_t i)                        { return json(json_array_get(    get(), i));          }
    bool set(         size_t i,  const json &j)        { const auto a = _a_get();
                                                         return a && (i == json_array_size(a)
                                                                      ? !json_array_append(a, j.get())
                                                                      : !json_array_set(a, i, j.get()));     }
    bool insert(      size_t i,  const json &j)        { return     !json_array_insert( get(), i, j.get());  }
    bool erase(          int i)                        { return     !json_array_remove( get(), i);           }
    bool erase(       size_t i)                        { return     !json_array_remove( get(), i);           }
    bool aclear()                                      { return     !json_array_clear(  get());              }
    bool push(                   const json &aj)       { return     !json_array_append( get(),    aj.get()); }
    bool concat(                 const json &aj)       { return     !json_array_extend( get(),    aj.get()); }

    // deep walk

    json path(const std::string &path);

    typedef std::function<bool (json_t *parent, const char *key, json_t *value)> visitor_func_t;
    void visit(const visitor_func_t &visitor);

    // variadic deep walk - type-aware, mutation-capable

    template <typename K1, typename K2, typename... Ks>
    json get(K1 &&k1, K2 &&k2, Ks&&... ks) {
        auto j2 = get(std::forward<K1>(k1));
        if (!j2)
            return json();
        return j2.get(std::forward<K2>(k2), std::forward<Ks>(ks)...);
    }

    template <typename K1, typename K2, typename T3, typename... Ts>
    bool set(K1 &&k1, K2 &&k2, T3 &&p3, Ts&&... ps) {
        if (!_ao_get(k1))
            return false;       // incompatible key type
        auto j2 = get(std::forward<K1>(k1));
        if (!j2) {
            j2._ao_get(k2);     // can't fail
            if (!set(std::forward<K1>(k1), j2))
                return false;   // should never happen
        }
        return j2.set(std::forward<K2>(k2), std::forward<T3>(p3), std::forward<Ts>(ps)...);
    }

    //
    // interfaces specific to object and array
    //

    // iterators

    class object_iterator {
        friend class json;

        json_t * const _obj;
        void *_it;
        object_iterator(json_t *obj, void *it) : _obj(obj), _it(it) {}

        static object_iterator make_begin(json_t *obj) { return object_iterator{obj, json_object_iter(obj)}; }
        static object_iterator make_end(json_t *obj)   { return object_iterator{obj, nullptr}; }

      public:
        typedef const char *key_type;
        typedef json mapped_type;
        typedef std::pair<const key_type, mapped_type> value_type;
        typedef std::initializer_list<value_type> init_type;

        value_type operator * () const {
            return value_type(json_object_iter_key(_it), json(json_object_iter_value(_it)));
        }
        object_iterator & operator ++ () {
            _it = json_object_iter_next(_obj, _it);
            return *this;
        }

        bool operator == (const object_iterator &right) const { return _it == right._it; }
        bool operator != (const object_iterator &right) const { return _it != right._it; }
    };

    object_iterator obegin() { return object_iterator::make_begin(get()); }
    object_iterator oend()   { return object_iterator::make_end(get()); }


    class array_iterator {
        friend class json;

        json_t * const _arr;
        size_t _i;
        array_iterator(json_t *arr, size_t i) : _arr(arr), _i(i) {}

        static array_iterator make_begin(json_t *arr) { return array_iterator{arr, 0}; }
        static array_iterator make_end(json_t *arr)   { return array_iterator{arr, json_array_size(arr)}; }

      public:
        typedef json value_type;
        typedef std::initializer_list<value_type> init_type;

        json operator * () const {
            return json(json_array_get(_arr, _i));
        }
        array_iterator & operator ++ () {
            ++_i;
            return *this;
        }

        array_iterator operator + (ssize_t right)         const { return array_iterator(_arr, _i + right); }
        array_iterator operator - (ssize_t right)         const { return array_iterator(_arr, _i - right); }
        ssize_t operator - (const array_iterator &right)  const { return (ssize_t)_i - (ssize_t)right._i; }
        bool operator == (const array_iterator &right)    const { return _i == right._i; }
        bool operator != (const array_iterator &right)    const { return _i != right._i; }
        bool operator <  (const array_iterator &right)    const { return _i <  right._i; }
    };

    array_iterator abegin()  { return array_iterator::make_begin(get()); }
    array_iterator aend()    { return array_iterator::make_end(get()); }

    // foreach-compatible pseudocontainer views; use with foreach loops:
    //    for (const auto &elem : j.arr()) {}
    //    for (const auto &kv   : j.obj()) {}

    template <class Iter>
    class aggregate_view {
        friend class json;

        json_ptr _p;
        explicit aggregate_view(json_ptr p) : _p(std::move(p)) {}

      public:
        using iterator = Iter;

        iterator begin()  { return Iter::make_begin(_p.get()); }
        iterator end()    { return Iter::make_end(_p.get()); }
    };

    using obj_view = aggregate_view<object_iterator>;
    using arr_view = aggregate_view<array_iterator>;

    obj_view obj()  { return obj_view(_p); }
    arr_view arr()  { return arr_view(_p); }
};


//----------------------------------------------------------------
// to_json(), by analogy with to_string()
//

inline json to_json(const json & j)   { return j; }
inline json to_json(      json &&j)   { return std::move(j); }
inline json to_json(const char *s)    { return json(s); }
inline json to_json(const std::string &s)  { return json(s); }
inline json to_json(int i)            { return json(i); }
inline json to_json(long i)           { return json(i); }
inline json to_json(long long i)      { return json(i); }
inline json to_json(unsigned u)       { return json(u); }
#if ULONG_MAX < LLONG_MAX
inline json to_json(unsigned long u)  { return json(u); }
#endif
inline json to_json(float f)          { return json(f); }
inline json to_json(double d)         { return json(d); }
inline json to_json(bool b)           { return json(b); }


//----------------------------------------------------------------
// metaprogramming
//
// ends:
//   make_json(T)    -> json
//   json_cast<T>(j) -> T
//
// means:
//   json_traits<T>
//

// default = failure: conversions do not work for unspecified types

template <class T> struct json_traits {
    typedef T type;
    static constexpr bool can_make = false;    // trait<T>::make() -> json works
    static constexpr bool can_cast = false;    // trait<T>::cast() -> T    works
};

// convenience base class for most convertible types

namespace impl {
template <class T> struct json_traits_conv {
    typedef T type;
    static constexpr bool can_make = true;
    static constexpr bool can_cast = true;

    static json make(T t)  { return json(t); }  // default for most cases
    static T cast(const json &j);               // default for most cases (see json.cc)
};
} // impl

// identity conversions

template <> struct json_traits<json> : impl::json_traits_conv<json> {
    static json make(const json  &j)  { return j; }
    static json cast(const json  &j)  { return j; }

    static json make(      json &&j)  { return std::move(j); }
    static json cast(      json &&j)  { return std::move(j); }
};

// string conversions

template <> struct json_traits<std::string> : impl::json_traits_conv<std::string> {
    static json make(const std::string &s)  { return json(s); }
};

// const char * conversions

template <> struct json_traits<const char *> : impl::json_traits_conv<const char *> {
    static json make(const char *s)         { return json(s); }
};

// integral conversions

template <> struct json_traits<short         > : impl::json_traits_conv<short         > {};
template <> struct json_traits<int           > : impl::json_traits_conv<int           > {};
template <> struct json_traits<long          > : impl::json_traits_conv<long          > {};
template <> struct json_traits<long long     > : impl::json_traits_conv<long long     > {};
template <> struct json_traits<unsigned short> : impl::json_traits_conv<unsigned short> {};
template <> struct json_traits<unsigned      > : impl::json_traits_conv<unsigned      > {};
#if ULONG_MAX < LLONG_MAX
template <> struct json_traits<unsigned long > : impl::json_traits_conv<unsigned long > {};
#endif

// real conversions

template <> struct json_traits<double> : impl::json_traits_conv<double> {};
template <> struct json_traits<float>  : impl::json_traits_conv<float>  {};

// boolean conversions

template <> struct json_traits<bool> : impl::json_traits_conv<bool> {};


//
// the above dance dance of templates exists to support this:
//   make_json<T>(t) -> json
//   json_cast<T>(j) -> T
//

// make_json<> function, rather like to_json() but with a precise type

template <class T>
inline typename std::enable_if<json_traits<T>::can_make, json>::type
make_json(T &&t) {
    return json_traits<T>::make(std::forward<T>(t));
}

// json_cast<> function, a la lexical_cast<>

template <class T>
inline typename std::enable_if<json_traits<T>::can_cast, T>::type
json_cast(const json &j) {
    return json_traits<T>::cast(j);
}


} // ten

#endif // LIBTEN_JSON_HH
