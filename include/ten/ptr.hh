#ifndef PTR_HH
#define PTR_HH

// ======================================================================
//
// ptr: A pointer that is nearly oblivious to its pointee
// (original name: exempt_ptr)
// http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2013/n3514.pdf
// heavily reformatted by Chip
//
// ======================================================================

#include <cstddef>     // nullptr_t
#include <functional>  // less
#include <type_traits> // add_pointer, enable_if, ...
#include <utility>     // swap

namespace ten {

// ======================================================================
// interface:

template <class E> class ptr;

template <class E> void swap(ptr<E> &, ptr<E> &) noexcept;

template <class E> ptr<E> make_ptr(E *) noexcept;

template <class E> bool operator == (ptr<E> const &, ptr<E> const &);
template <class E> bool operator != (ptr<E> const &, ptr<E> const &);
template <class E> bool operator == (ptr<E> const &, std::nullptr_t) noexcept;
template <class E> bool operator != (ptr<E> const &, std::nullptr_t) noexcept;
template <class E> bool operator == (std::nullptr_t, ptr<E> const &) noexcept;
template <class E> bool operator != (std::nullptr_t, ptr<E> const &) noexcept;
template <class E> bool operator <  (ptr<E> const &, ptr<E> const &);
template <class E> bool operator >  (ptr<E> const &, ptr<E> const &);
template <class E> bool operator <= (ptr<E> const &, ptr<E> const &);
template <class E> bool operator >= (ptr<E> const &, ptr<E> const &);

// ======================================================================
// implementation:

template <class E>
class ptr {
  public:
    // publish our template parameter and variations thereof:
    using element_type = E;
    using pointer = typename std::add_pointer<E>::type;
    using reference = typename std::add_lvalue_reference<E>::type;

  private:
    template <class P>
    static constexpr bool is_compat() {
        return std::is_convertible< typename std::add_pointer<P>::type
                                    , pointer
                                  >::value;
    }

  public:
//construction:
    constexpr ptr() noexcept : p() {}
    constexpr ptr(std::nullptr_t) noexcept : p() {}
    explicit ptr(pointer other) noexcept : p(other) {}

    template <class E2, class = typename std::enable_if< is_compat<E2>() >::type>
    explicit ptr(E2 *other) noexcept : p(other) {}

    template <class E2, class = typename std::enable_if< is_compat<E2>() >::type>
    ptr(ptr<E2> const &other) noexcept : p(other.get()) {}

//assignment:
    ptr & operator = (std::nullptr_t) noexcept { reset(nullptr); return *this; }

    template <class E2>
    typename std::enable_if< is_compat<E2>(), ptr & >::type
    operator = (E2 *other) noexcept
        { reset(other); return *this; }

    template <class E2>
    typename std::enable_if< is_compat<E2>(), ptr & >::type
    operator = (ptr<E2> const &other) noexcept
        { reset(other.get()); return *this; }

// observers:
    pointer get() const noexcept { return p; }
    reference operator * () const noexcept { return *get(); }
    pointer operator -> () const noexcept { return get(); }
    explicit operator bool () const noexcept { return get(); }
    bool operator ! () const noexcept { return !get(); }
// modifiers:
    pointer release() noexcept { pointer old = get(); reset(); return old; }
    void reset(pointer t = nullptr) noexcept { p = t; }
    void swap(ptr &other) noexcept { std::swap(p, other.p); }
  private:
    pointer p;
};

// -----------------------------------------------
// non-member swap:
template <class E> inline void swap(ptr<E> &x, ptr<E> &y) noexcept { x.swap(y); }

// -----------------------------------------------
// non-member make_ptr:
template <class E> inline ptr<E> make_ptr(E *p) noexcept { return ptr<E>{p}; }

// -----------------------------------------------
// non-member (in)equality comparison:
template <class E> inline bool operator == (ptr<E> const &x, ptr<E> const &y) { return x.get() == y.get(); }
template <class E> inline bool operator != (ptr<E> const &x, ptr<E> const &y) { return x.get() != y.get(); }
template <class E> inline bool operator == (ptr<E> const &x, std::nullptr_t)  noexcept { return !x; }
template <class E> inline bool operator != (ptr<E> const &x, std::nullptr_t)  noexcept { return  x; }
template <class E> inline bool operator == (std::nullptr_t,  ptr<E> const &y) noexcept { return !y; }
template <class E> inline bool operator != (std::nullptr_t,  ptr<E> const &y) noexcept { return  y; }

// -----------------------------------------------
// non-member ordering:
template <class E>
inline bool operator < (ptr<E> const &x, ptr<E> const &y) {
    using PTR = typename ptr<E>::pointer;
    return std::less<PTR>()(x.get(), y.get());
}
template <class E> inline bool operator >  (ptr<E> const &x, ptr<E> const &y) { return y < x; }
template <class E> inline bool operator <= (ptr<E> const &x, ptr<E> const &y) { return !(y < x); }
template <class E> inline bool operator >= (ptr<E> const &x, ptr<E> const &y) { return !(x < y); }

} // end namespace ten

#endif // PTR_HH
