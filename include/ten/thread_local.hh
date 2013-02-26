#ifndef LIBTEN_THREAD_LOCAL_HH
#define LIBTEN_THREAD_LOCAL_HH

#include <type_traits>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace ten {

// TODO: use kernel::is_main_thread after default_stacksize is gone
inline bool is_main_thread() noexcept {
    return getpid() == syscall(SYS_gettid);
}

//! thread local storage because thread_local keyword isn't well supported yet
template <class Unique, class T>
class thread_cached {
private:
    typedef typename std::aligned_storage<sizeof(T),
            std::alignment_of<T>::value>::type storage_type;

    static __thread storage_type storage;
    static __thread bool allocated;

    static void atexit() {
        placement_delete(&storage);
    }

    static void placement_delete(void *v) {
        static_cast<T*>(v)->~T();
        allocated = false;
    }

public:
    T *get() {
        if (!allocated) {
            new (&storage) T();
            allocated = true;
            if (is_main_thread()) {
                std::atexit(thread_cached::atexit);
            } else {
                static pthread_key_t key;
                pthread_key_create(&key, thread_cached::placement_delete);
                pthread_setspecific(key, &storage);
            }
        }
        return reinterpret_cast<T*>(&storage);
    }

    T *operator-> () {
        return get();
    }
};

template <class Unique, class T>
__thread typename thread_cached<Unique, T>::storage_type thread_cached<Unique, T>::storage;
template <class Unique, class T>
__thread bool thread_cached<Unique, T>::allocated = false;

} // ten

#endif
