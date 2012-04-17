#ifndef LIBTEN_MAYBE_HH
#define LIBTEN_MAYBE_HH

#include <exception>
#include <functional>
#include <utility>

namespace ten {
using std::move;

//----------------------------------------------------------------
// maybe<>
//

enum nothing_t { nothing };

class nothing_exception : public std::exception {
  public:
    const char *what() const throw()  { return "maybe<> is nothing"; }
};

template <class T> class maybe {
    union {
        char _buf[sizeof(T)];
        T    _val;
    };
    bool _ok;

  public:
    typedef T value_type;

    maybe()                               : _ok() {}
    maybe(nothing_t)                      : _ok() {}
    maybe(const T  &val)                  : _ok() { reset(val); }
    maybe(      T &&val)                  : _ok() { reset(val); }
    maybe(const maybe  &m)                : _ok() { if (m.ok()) reset(     m.get_ref() ); }
    maybe(      maybe &&m)                : _ok() { if (m.ok()) reset(move(m.get_ref())); }
    maybe & operator = (const maybe  &m)  { if (m.ok()) reset(     m.get_ref() ); else reset(); return *this; }
    maybe & operator = (const maybe &&m)  { if (m.ok()) reset(move(m.get_ref())); else reset(); return *this; }
    maybe & operator = (nothing_t)        { reset(); return *this; }
    ~maybe()                              { reset(); }

    bool ok() const           { return _ok; }
          T *get_ptr()        { if (!_ok) return nullptr; return &_val; }
    const T *get_ptr() const  { if (!_ok) return nullptr; return &_val; }
          T &get_ref()        { if (!_ok) throw nothing_exception(); return _val; }
    const T &get_ref() const  { if (!_ok) throw nothing_exception(); return _val; }
          T  get()     const  { if (!_ok) throw nothing_exception(); return _val; }

    void reset(const T  &v)  { if (_ok) { _val = v; } else { new (_buf) T(v); _ok = true; } }
    void reset(      T &&v)  { if (_ok) { _val = v; } else { new (_buf) T(v); _ok = true; } }
    void reset()             { if (_ok) { _ok = false; _val.T::~T(); } }
};

template <class T>
struct hash : public std::unary_function<const maybe<T> &, size_t> {
    size_t operator ()(const maybe<T> &m) {
        using std::hash;
        return m.ok() ? hash<T>(m.get_ref()) : 0;
    }
};

} // ten

#endif // LIBTEN_MAYBE_HH
