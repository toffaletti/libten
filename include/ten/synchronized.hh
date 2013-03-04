#ifndef LIBTEN_SYNCHRONIZED_HH
#define LIBTEN_SYNCHRONIZED_HH

#include <mutex>
#include <type_traits>

namespace ten {

template <typename VT, typename Mutex = std::mutex>
class synchronized_view {
    std::unique_lock<Mutex> _lk;
    VT &_vt;

public:
    synchronized_view(Mutex &m, VT &vt) : _lk(m), _vt(vt) {}

    VT *get() const           { return &_vt; }
    VT * operator -> () const { return &_vt; }
    VT & operator * () const  { return  _vt; }
};

template <typename T, typename Mutex = std::mutex>
class synchronized {
    mutable Mutex _m;
    T _v;

public:
    using mutex_type = Mutex;
    using guard_type = std::lock_guard<Mutex>;
    using lock_type  = std::unique_lock<Mutex>;

    using view       = synchronized_view<      T, Mutex>;
    using const_view = synchronized_view<const T, Mutex>;

    friend view       sync_view(      synchronized &s) { return       view(s._m, s._v); }
    friend const_view sync_view(const synchronized &s) { return const_view(s._m, s._v); }

    synchronized() {}

    // This is not quite type-safe, but it can only go wrong if you nest synchronized<>.
    // So don't do that.
    template <typename ...Args>
        synchronized(Args&&... args) : _v(std::forward<Args>(args)...) {}

    synchronized(const synchronized &other) = delete;
    synchronized(synchronized &&other)      = delete;

    // Watch out for snakes... er, deadlocks.  Lock in consistent order.
    synchronized & operator = (const synchronized &other) {
        if (&other != this) {
            guard_type g1(other._m), g2(_m);
            _v = other.v;
        }
        return *this;
    }
    synchronized & operator = (synchronized &&other) {
        if (&other != this) {
            guard_type g1(other._m), g2(_m);
            _v = std::move(other.v);
        }
        return *this;
    }

    template <typename Func, typename Ret = typename std::result_of<Func(T&)>::type>
        auto operator () (Func &&f)       -> Ret
            { guard_type g(_m); return f(_v); }

    template <typename Func, typename Ret = typename std::result_of<Func(T&)>::type> // not const T.  on purpose.
        auto operator () (Func &&f) const -> Ret
            { guard_type g(_m); return f(_v); }

    template <typename Func>
        friend auto sync(      synchronized &s, Func &&f) -> typename std::result_of<Func(T&)>::type
            { guard_type g(s._m); return f(s._v); }

    template <typename Func>
        friend auto sync(const synchronized &s, Func &&f) -> typename std::result_of<Func(T&)>::type // not const T.  on purpose
            { guard_type g(s._m); return f(s._v); }
};


// write maybe_sync(T &t, ...) if you want to work with a T that may or may not be synchronized<>

template <typename T, typename Func>
    inline auto maybe_sync(      synchronized<T> &s, Func &&f) -> decltype(s(f))
        { return s(std::forward<Func>(f)); }

template <typename T, typename Func>
    inline auto maybe_sync(const synchronized<T> &s, Func &&f) -> decltype(s(f))
        { return s(std::forward<Func>(f)); }

template <typename T, class Func>
    inline auto maybe_sync(      T &t, Func &&f) -> decltype(f(t))
        { return f(t); }

template <typename T, class Func>
    inline auto maybe_sync(const T &t, Func &&f) -> decltype(f(t))
        { return f(t); }

} // ten

#endif
