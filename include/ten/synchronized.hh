#ifndef LIBTEN_SYNCHRONIZED_HH
#define LIBTEN_SYNCHRONIZED_HH

#include <mutex>

namespace ten {

template <typename T, typename Mutex = std::mutex> class synchronized {
protected:
    mutable Mutex _m;
    mutable T _v;
public:
    typedef Mutex mutex_type;
    synchronized() : _v{} {}
    template <typename ...Args> synchronized(Args... args) : _v{args...} {}

    template <typename Func, typename Sync>
        friend auto synchronize(const Sync &sync, Func &&f) -> decltype(f(sync._v)) {
            std::lock_guard<typename Sync::mutex_type> lock(sync._m);
            return f(sync._v);
        }
};

}

#endif
