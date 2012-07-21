#ifndef LIBTEN_SYNCHRONIZED_HH
#define LIBTEN_SYNCHRONIZED_HH

#include <mutex>

namespace ten {

template <typename T, typename Mutex = std::mutex> class synchronized {
protected:
    Mutex _m;
    T _v;
public:
    typedef Mutex mutex_type;
    synchronized() {}
    template <typename ...Args> synchronized(Args... args) : _v(args...) {}

    template <typename Func, typename Sync> friend void synchronize(Sync &sync, Func &&f) {
        std::lock_guard<typename Sync::mutex_type> lock(sync._m);
        f(sync._v);
    }
};

}

#endif
