#ifndef LIBTEN_MAYBE_HH
#define LIBTEN_MAYBE_HH

#include <exception>
#include <functional>
#include <utility>
#include <ostream>

namespace ten {

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
    maybe(const T  &val)                  : _ok() { reset(          val ); }
    maybe(      T &&val)                  : _ok() { reset(std::move(val)); }
    maybe(const maybe  &m)                : _ok() { if (m.ok()) reset(          m.get_ref() ); }
    maybe(      maybe &&m)                : _ok() { if (m.ok()) reset(std::move(m.get_ref())); }
    maybe & operator = (const maybe  &m)  { if (m.ok()) reset(          m.get_ref() ); else reset(); return *this; }
    maybe & operator = (      maybe &&m)  { if (m.ok()) reset(std::move(m.get_ref())); else reset(); return *this; }
    maybe & operator = (nothing_t)        { reset(); return *this; }
    ~maybe()                              { reset(); }

    bool ok() const           { return _ok; }
          T *get_ptr()        { if (!_ok) return nullptr; return &_val; }
    const T *get_ptr() const  { if (!_ok) return nullptr; return &_val; }
          T &get_ref()        { if (!_ok) throw nothing_exception(); return _val; }
    const T &get_ref() const  { if (!_ok) throw nothing_exception(); return _val; }
          T  get()     const  { if (!_ok) throw nothing_exception(); return _val; }

    void reset(const T  &v)  { if (_ok) { _val = v;            } else { new (_buf) T(v);            _ok = true; } }
    void reset(      T &&v)  { if (_ok) { _val = std::move(v); } else { new (_buf) T(std::move(v)); _ok = true; } }
    void reset()             { if (_ok) { _ok = false; _val.T::~T(); } }

    template <typename ...Args>
    void emplace(Args&& ...args) {
        if (_ok)
            _val = T(std::forward<Args>(args)...);
        else {
            new (_buf) T(std::forward<Args>(args)...);
            _ok = true;
        }
    }

    friend std::ostream &operator << (std::ostream &o, const maybe<T> &m) {
        if (m.ok()) {
            o << m._val;
        } else {
            o << "null";
        }
        return o;
    }
};

} // ten

namespace std {
template <class T> struct hash<ten::maybe<T>> : public unary_function<const ten::maybe<T> &, size_t> {
    size_t operator ()(const ten::maybe<T> &m) {
        return m.ok() ? hash<T>(m.get_ref()) : 0;
    }
};
} // std

#endif // LIBTEN_MAYBE_HH
