#ifndef LIBTEN_PTR_HH
#define LIBTEN_PTR_HH

#include <cstddef>
#include <type_traits>
#include <utility>

namespace ten {

// a pointer that doesn't do pointer arithmetic and won't automatically
// convert to anything dangerous

template <class T> class ptr {
    T *_t;

    template <class U>
      static constexpr bool _compat()
        { return std::is_convertible<U *, T *>::value; }

public:
    using target = T;
    using pointer = T *;
    using reference = T &;

    constexpr ptr()               noexcept : _t{} {}
    constexpr ptr(std::nullptr_t) noexcept : _t{} {}
    constexpr explicit ptr(T *t)  noexcept : _t{t} {}
    ptr & operator = (std::nullptr_t) { _t = nullptr; return *this; }

    ptr(const ptr &other)    { reset(other.get()); }
    template <class U,
              class = typename std::enable_if<_compat<U>()>::type>
      ptr(const ptr<U> &other) { reset(other.get()); }
    template <class U,
              class = typename std::enable_if<_compat<U>()>::type>
      explicit ptr(U *u)       { reset(u); }

    ptr & operator = (const ptr &other)     { reset(other.get()); return *this; }
    template <class U,
              class = typename std::enable_if<_compat<U>()>::type>
      ptr & operator = (const ptr<U> &other)  { reset(other.get()); return *this; }

    template <class U,
              class = typename std::enable_if<_compat<U>()>::type>
      ptr & operator = (U *u)                 { reset(u); return *this; }

    pointer get()                  const noexcept  { return _t; }
    void reset(pointer t = nullptr)      noexcept  { _t = t; }
    pointer release()                    noexcept  { auto t = _t; reset(); return t; }

    pointer   operator -> ()  const noexcept  { return  get(); }
    reference operator *  ()  const noexcept  { return *get(); }
    explicit operator bool () const noexcept  { return  get(); }
    bool operator ! ()        const noexcept  { return !get(); }

    bool operator == (const ptr &other) const noexcept  { return get() == other.get(); }
    bool operator != (const ptr &other) const noexcept  { return get() != other.get(); }
    bool operator <  (const ptr &other) const noexcept  { return get() <  other.get(); }
    bool operator <= (const ptr &other) const noexcept  { return get() <= other.get(); }
    bool operator >  (const ptr &other) const noexcept  { return get() >  other.get(); }
    bool operator >= (const ptr &other) const noexcept  { return get() >= other.get(); }

    friend bool operator != (const ptr &p, std::nullptr_t) noexcept  { return p; }
    friend bool operator != (std::nullptr_t, const ptr &p) noexcept { return p; }

    friend bool operator == (const ptr &p, std::nullptr_t) noexcept { return !p; }
    friend bool operator == (std::nullptr_t, const ptr &p) noexcept { return !p; }

    friend void swap(ptr &left, ptr &right) noexcept { std::swap(left._t, right._t); }
};

template <class T>
  inline ptr<T> make_ptr(T *t) noexcept  { return ptr<T>(t); }


} // namespace ten

#endif // LIBTEN_PTR_HH
