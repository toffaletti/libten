#ifndef LIBTEN_JSON_HH
#define LIBTEN_JSON_HH

#include <jansson.h>
#include <string.h>
#include <string>
#include <functional>

#include <limits.h>
#ifndef JSON_INTEGER_IS_LONG_LONG
# error Y2038
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 4)
# define TEN_JSON_CXX11
#endif

namespace ten {
using namespace std;

#ifndef TEN_JSON_CXX11
const intptr_t nullptr = 0;
#endif


//----------------------------------------------------------------
// streams meet json_t
//

ostream & operator << (ostream &o, const json_t *j);


//----------------------------------------------------------------
// shared_json_ptr<>
//

enum json_take_t { json_take };

template <class J> class shared_json_ptr {
private:
    J *p;

    typedef void (shared_json_ptr::*unspecified_bool_type)() const;
    void true_value() const {}

public:
    // ctor, dtor, assign
    shared_json_ptr()                          : p() {}
    explicit shared_json_ptr(J *j)             : p(json_incref(j)) {}
    shared_json_ptr(J *j, json_take_t)         : p(j) {}
    ~shared_json_ptr()                         { json_decref(p); }

    // construction and assignment - make the compiler work for a living
                        shared_json_ptr(              const shared_json_ptr      &jp) : p(json_incref(jp.get())) {}
    template <class J2> shared_json_ptr(              const shared_json_ptr<J2>  &jp) : p(json_incref(jp.get())) {}
    template <class J2> shared_json_ptr(                    shared_json_ptr<J2> &&jp) : p(jp.release())          {}
                        shared_json_ptr & operator = (const shared_json_ptr      &jp) { reset(jp.get());    return *this; }
    template <class J2> shared_json_ptr & operator = (const shared_json_ptr<J2>  &jp) { reset(jp.get());    return *this; }
    template <class J2> shared_json_ptr & operator = (      shared_json_ptr<J2> &&jp) { take(jp.release()); return *this; }

    // object interface

    void reset(J *j) { take(json_incref(j)); }
    void take(J *j)  { json_decref(p); p = j; }  // must be convenient for use with C code

    J * operator -> () const { return p; }
    J *get() const           { return p; }
    J *release()             { J *j = p; p = nullptr; return j; }

    operator unspecified_bool_type () const {
        return p ? &shared_json_ptr::true_value : nullptr;
    }

    friend ostream & operator << (ostream &o, const shared_json_ptr &jp) { return o << jp.get(); }
};

typedef shared_json_ptr<      json_t>       json_ptr;
typedef shared_json_ptr<const json_t> const_json_ptr;

template <class J> shared_json_ptr<J> make_json_ptr(J *j) { return shared_json_ptr<J>(j); }
template <class J> shared_json_ptr<J> take_json_ptr(J *j) { return shared_json_ptr<J>(j, json_take); }


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
    json_ptr _p;

    typedef void (json::*unspecified_bool_type)() const;
    void true_value() const {}

  public:
    // construction and assignment,
    //   including a selection of conversions from basic types

    json()                                     : _p()             {}
    json(const json  &js)                      : _p(js._p)        {}
    json(      json &&js)                      : _p(move(js._p))  {}
    json(const json_ptr  &p)                   : _p(p)            {}
    json(      json_ptr &&p)                   : _p(p)            {}
    explicit json(json_t *j)                   : _p(j)            {}
             json(json_t *j, json_take_t t)    : _p(j, t)         {}
    json & operator = (const json  &js)        { _p = js._p;       return *this; }
    json & operator = (      json &&js)        { _p = move(js._p); return *this; }

    json(const char *s)                        : _p(json_string(s),                 json_take) {}
    json(const string &s)                      : _p(json_string(s.c_str()),         json_take) {}
    json(int i)                                : _p(json_integer(i),                json_take) {}
    json(long i)                               : _p(json_integer(i),                json_take) {}
    json(long long i)                          : _p(json_integer(i),                json_take) {}
    json(unsigned u)                           : _p(json_integer(u),                json_take) {}
#if ULONG_MAX < LLONG_MAX
    json(unsigned long u)                      : _p(json_integer(u),                json_take) {}
#endif
    json(double r)                             : _p(json_real(r),                   json_take) {}
    json(bool b)                               : _p(b ? json_true() : json_false(), json_take) {}

    static json object()                 { return json(json_object(),   json_take); }
    static json array()                  { return json(json_array(),    json_take); }
    static json str(const char *s)       { return json(s); }
    static json str(const string &s)     { return json(s); }
    static json integer(int i)           { return json(i); }
    static json integer(long i)          { return json(i); }
    static json integer(long long i)     { return json(i); }
    static json integer(unsigned u)      { return json(u); }
#if ULONG_MAX < LLONG_MAX
    static json integer(unsigned long u) { return json(u); }
#endif
    static json real(double d)           { return json(d); }
    static json boolean(bool b)          { return json(b); }
    static json jtrue()                  { return json(json_true(),     json_take); }
    static json jfalse()                 { return json(json_false(),    json_take); }
    static json null()                   { return json(json_null(),     json_take); }

    // default to building objects, they're more common
    json(initializer_list<pair<const char *, json>> init)
        : _p(json_object(), json_take)
    {
#ifdef TEN_JSON_CXX11
        for (const auto &kv : init)
            set(kv.first, kv.second);
#else
        for (auto i = init.begin(); i != init.end(); ++i) {
            set(i->first, i->second);
        }
#endif
    }
    static json array(initializer_list<const json> init) {
        json a(json_array(), json_take);
#ifdef TEN_JSON_CXX11
        for (const auto &j : init)
            a.push(j);
#else
        for (auto i = init.begin(); i != init.end(); ++i) {
            a.push(*i);
        }
#endif
        return a;
    }

    // manipulation via json_t*

    friend json take_json(json_t *j)           { return json(j, json_take); }
    void take(json_t *j)                       { _p.take(j); }
    void reset(json_t *j)                      { _p.reset(j); }
    void reset(const json_ptr &p)              { _p = p; }
    json_t *get() const                        { return _p.get(); }
    json_t *release()                          { return _p.release(); }
    json_ptr get_shared() const                { return _p; }

    // equivalence

    friend bool operator == (const json &lhs, const json &rhs) {
        return json_equal(lhs._p.get(), rhs._p.get());
    }
    friend bool operator != (const json &lhs, const json &rhs) {
        return !(lhs == rhs);
    }

    // parse and ouput

    static json load(const string &s, unsigned flags = JSON_DECODE_ANY)  { return load(s.data(), s.size(), flags); }
    static json load(const char *s, unsigned flags = JSON_DECODE_ANY)    { return load(s, strlen(s), flags); }
    static json load(const char *s, size_t len, unsigned flags);

    string dump(unsigned flags = JSON_ENCODE_ANY);

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

    string str()         const  { return json_string_value(get()); }
    const char *c_str()  const  { return json_string_value(get()); }
    json_int_t integer() const  { return json_integer_value(get()); }
    double real()        const  { return json_real_value(get()); }
    bool boolean()       const  { json_t *j = get(); return j && !json_is_null(j) && !json_is_false(j); }

    operator unspecified_bool_type () const {
        return get() ? &json::true_value : nullptr;
    }

    bool set_integer(json_int_t iv)   { return !json_integer_set(get(), iv); }
    bool set_real(double rv)          { return !json_real_set(   get(), rv); }

    // aggregate access

    template <class KEY> json operator [] (KEY key)     { return get(key); }

    size_t osize() const                                { return      json_object_size(   get());                    }
    json get(           const char *key)                { return json(json_object_get(    get(), key));              }
    json get(         const string &key)                { return json(json_object_get(    get(), key.c_str()));      }
    bool set(           const char *key, const json &j) { return     !json_object_set(    get(), key,         j.get()); }
    bool set(         const string &key, const json &j) { return     !json_object_set(    get(), key.c_str(), j.get()); }
    bool erase(         const char *key)                { return     !json_object_del(    get(), key);               }
    bool erase(       const string &key)                { return     !json_object_del(    get(), key.c_str());       }
    bool oclear()                                       { return     !json_object_clear(  get());                    }
    bool omerge(         const json &oj)                { return     !json_object_update(          get(), oj.get()); }
    bool omerge_existing(const json &oj)                { return     !json_object_update_existing( get(), oj.get()); }
    bool omerge_missing( const json &oj)                { return     !json_object_update_missing(  get(), oj.get()); }

    size_t asize() const                           { return      json_array_size(   get());              }
    json get(            int i)                    { return json(json_array_get(    get(), i));          }
    json get(         size_t i)                    { return json(json_array_get(    get(), i));          }
    bool set(         size_t i,  const json &j)    { return     !json_array_set(    get(), i, j.get());  }
    bool insert(      size_t i,  const json &j)    { return     !json_array_insert( get(), i, j.get());  }
    bool erase(       size_t i)                    { return     !json_array_remove( get(), i);           }
    bool arr_clear()                               { return     !json_array_clear(  get());              }
    bool push(                   const json &aj)   { return     !json_array_append( get(),    aj.get()); }
    bool concat(                 const json &aj)   { return     !json_array_extend( get(),    aj.get()); }

    // deep walk

    json path(const string &path);

    typedef function<bool (json_t *parent, const char *key, json_t *value)> visitor_func_t;
    void visit(const visitor_func_t &visitor);

    // interfaces specific to object and array

    class object_iterator {
        json_t *_obj;
        void *_it;

        friend class json;
        explicit object_iterator(json_t *obj)           : _obj(obj), _it(json_object_iter(obj)) {}
                 object_iterator(json_t *obj, void *it) : _obj(obj), _it(it) {}

      public:
        typedef const char *key_type;
        typedef json mapped_type;
        typedef pair<const key_type, mapped_type> value_type;
        typedef initializer_list<value_type> init_type;

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

    class array_iterator {
        json_t *_arr;
        size_t _i;

        friend class json;
        explicit array_iterator(json_t *arr, size_t i = 0) : _arr(arr), _i(i) {}

      public:
        typedef json value_type;
        typedef initializer_list<value_type> init_type;

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


    // if you want to iterate without .arr() and .obj(), here you go

    object_iterator obegin() { return object_iterator(get()); }
    object_iterator oend()   { return object_iterator(get(), nullptr); }

    array_iterator  abegin() { return array_iterator( get()); }
    array_iterator  aend()   { return array_iterator( get(), json_array_size(get())); }


    // if you want to iterate with .arr() and .obj(), here you go

    class obj_view {
        json_t *_obj;

        friend class json;
        explicit obj_view(json_t *obj) : _obj(obj) {}

      public:
        typedef object_iterator iterator;
        iterator begin() const { return iterator(_obj); }
        iterator end()   const { return iterator(_obj, nullptr); }
    };
    obj_view obj()  { return obj_view(get()); }

    class arr_view {
        json_t *_arr;

        friend class json;
        explicit arr_view(json_t *arr) : _arr(arr) {}

      public:
        typedef array_iterator iterator;
        iterator begin() const { return iterator(_arr); }
        iterator end()   const { return iterator(_arr, json_array_size(_arr)); }
    };
    arr_view arr()  { return arr_view(get()); }
};

inline ostream & operator << (ostream &o, const json &j) { return o << j.get(); }


//----------------------------------------------------------------
// to_json(), by analogy with to_string()
//

inline json to_json(const char *s)    { return json(s); }
inline json to_json(const string &s)  { return json(s); }
inline json to_json(int i)            { return json(i); }
inline json to_json(long i)           { return json(i); }
inline json to_json(long long i)      { return json(i); }
inline json to_json(unsigned u)       { return json(u); }
#if ULONG_MAX < LLONG_MAX
inline json to_json(unsigned long u)  { return json(u); }
#endif
inline json to_json(double d)         { return json(d); }
inline json to_json(bool b)           { return json(b); }

} // ten

#endif // LIBTEN_JSON_HH
