#ifndef LIBTEN_JSON_HH
#define LIBTEN_JSON_HH

#include <jansson.h>
#include <string.h>
#ifndef JSON_INTEGER_IS_LONG_LONG
# error Y2038
#endif
#include <string>
#include <algorithm>
#include <functional>

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 4)
# define TEN_JSON_CXX11
#endif

namespace ten {
using namespace std;

#ifndef TEN_JSON_CXX11
const intptr_t nullptr = 0;
#endif

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
};

typedef shared_json_ptr<      json_t>       json_ptr;
typedef shared_json_ptr<const json_t> const_json_ptr;

template <class J> shared_json_ptr<J> make_json_ptr(J *j) { return shared_json_ptr<J>(j); }
template <class J> shared_json_ptr<J> take_json_ptr(J *j) { return shared_json_ptr<J>(j, json_take); }

ostream & operator << (ostream &o, const json_t *j);
inline ostream & operator << (ostream &o,       json_ptr jp) { return o << jp.get(); }
inline ostream & operator << (ostream &o, const_json_ptr jp) { return o << jp.get(); }


//----------------------------------------------------------------
// type-safe convenient json_t access
//

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
             json(json_t *j, json_take_t)      : _p(j, json_take) {}
    json & operator = (const json  &js)        { _p = js._p;       return *this; }
    json & operator = (      json &&js)        { _p = move(js._p); return *this; }

    json(const char *s)                        : _p(json_string(s),                 json_take) {}
    json(const string &s)                      : _p(json_string(s.c_str()),         json_take) {}
    json(json_int_t i)                         : _p(json_integer(i),                json_take) {}
    json(int i)                                : _p(json_integer(i),                json_take) {}
    json(double r)                             : _p(json_real(r),                   json_take) {}
    json(float r)                              : _p(json_real(r),                   json_take) {}
    json(bool b)                               : _p(b ? json_true() : json_false(), json_take) {}

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

    // construction from basic C++ types

    static json object()               { return json(json_object(),    json_take); }
    static json array()                { return json(json_array(),     json_take); }
    static json str(const char *s)     { return json(json_string(s),   json_take); }
    static json str(const string &s)   { return json(json_string(s.c_str()), json_take); }
    static json integer(json_int_t i)  { return json(json_integer(i),  json_take); }
    static json real(double f)         { return json(json_real(f),     json_take); }
    static json boolean(bool b)        { return json(b ? json_true() : json_false(), json_take); }
    static json null()                 { return json(json_null(),      json_take); }

    // type access

    json_type type()     const  { json_t *j = get(); return j ? json_typeof(j) : (json_type)-1; }
    bool is_object()     const  { return json_is_object(get()); }
    bool is_array()      const  { return json_is_array(get()); }
    bool is_aggregate()  const  { return is_array() || is_object(); }
    bool is_string()     const  { return json_is_integer(get()); }
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
    // there are more objects than arrays, so arrays get ()

    size_t osize() const                            { return json_object_size(get()); }
    json operator [] (  const char *key)            { return json(json_object_get(get(), key)); }
    json operator [] (const string &key)            { return json(json_object_get(get(), key.c_str())); }
    json oget(      const char *key)                { return json(json_object_get(get(), key)); }
    json oget(    const string &key)                { return json(json_object_get(get(), key.c_str())); }
    bool oset(      const char *key, const json &j) { return !json_object_set(    get(), key,         j.get()); }
    bool oset(    const string &key, const json &j) { return !json_object_set(    get(), key.c_str(), j.get()); }
    bool oerase(    const char *key)                { return !json_object_del(    get(), key); }
    bool oerase(  const string &key)                { return !json_object_del(    get(), key.c_str()); }
    bool oclear()                                   { return !json_object_clear(  get()); }
    bool omerge(         const json &j)             { return !json_object_update(          get(), j.get()); }
    bool omerge_existing(const json &j)             { return !json_object_update_existing( get(), j.get()); }
    bool omerge_missing( const json &j)             { return !json_object_update_missing(  get(), j.get()); }

    size_t asize() const                    { return json_array_size(get()); }
    json operator () (size_t i)             { return json(json_array_get(get(), i)); }
    json aget(    size_t i)                 { return json(json_array_get(get(), i)); }
    bool aset(    size_t i,  const json &j) { return !json_array_set(        get(), i,   j.get()); }
    bool ainsert( size_t i,  const json &j) { return !json_array_insert(     get(), i,   j.get()); }
    bool aerase(  size_t i)                 { return !json_array_remove(     get(), i);            }
    bool aclear()                           { return !json_array_clear(      get());               }
    bool apush(              const json &j) { return !json_array_append(     get(),      j.get()); }
    bool aconcat(            const json &j) { return !json_array_extend(     get(),      j.get()); }

    // deep walks

    json path(const string &path);

    typedef function<bool (json_t *parent, const char *key, json_t *value)> visitor_func_t;
    void visit(const visitor_func_t &visitor);

    // interfaces specific to object and array

    class object_iterator {
        friend class json;

        json_t *_obj;
        void *_it;
        object_iterator(json_t *obj)
            : _obj(obj), _it(json_object_iter(obj)) {}
        object_iterator(json_t *obj, void *it)
            : _obj(obj), _it(it) {}

      public:
        typedef const char *key_type;
        typedef json mapped_type;
        typedef pair<const key_type, mapped_type> value_type;

        value_type operator * () const {
            return value_type(json_object_iter_key(_it), json(json_object_iter_value(_it)));
        }
        object_iterator & operator ++ () {
            _it = json_object_iter_next(_obj, _it);
            return *this;
        }

        friend bool operator == (const object_iterator &left, const object_iterator &right) {
            return left._it == right._it;
        }
        friend bool operator != (const object_iterator &left, const object_iterator &right) {
            return !(left == right);
        }
    };

    class array_iterator {
        friend class json;

        json_t *_arr;
        size_t _i;
        array_iterator(json_t *arr, size_t i = 0) : _arr(arr), _i(i) {}

      public:
        typedef json value_type;

        json operator * () const { return json(json_array_get(_arr, _i)); }
        array_iterator & operator ++ () { ++_i; return *this; }

        friend bool operator == (const array_iterator &left, const array_iterator &right) {
            return left._arr == right._arr && left._i == right._i;
        }
        friend bool operator != (const array_iterator &left, const array_iterator &right) {
            return !(left == right);
        }
        friend ssize_t operator + (const array_iterator &left, const array_iterator &right) {
            return left._i + right._i;
        }
        friend ssize_t operator - (const array_iterator &left, const array_iterator &right) {
            return (ssize_t)left._i - (ssize_t)right._i;
        }
    };

    class obj_view {
        friend class json;
        json_t *_obj;
        obj_view(json_t *obj) : _obj(obj) {}
      public:
        typedef object_iterator iterator;
        iterator begin() { return iterator(_obj); }
        iterator end()   { return iterator(nullptr); }
    };
    obj_view obj() { return obj_view(get()); }

    class arr_view {
        friend class json;
        json_t *_arr;
        arr_view(json_t *arr) : _arr(arr) {}
      public:
        typedef array_iterator iterator;
        iterator begin() { return iterator(_arr); }
        iterator end()   { return iterator(_arr, json_array_size(_arr)); }
    };
    arr_view arr() { return arr_view(get()); }
};

inline ostream & operator << (ostream &o, const json &j) { return o << j.get(); }

} // ten

#endif // LIBTEN_JSON_HH
