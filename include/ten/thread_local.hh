#ifndef LIBTEN_THREAD_LOCAL_HH
#define LIBTEN_THREAD_LOCAL_HH

// TODO: dont really need this header if we move code around
#include "ten/task/runtime.hh"
#include <type_traits>
#include <pthread.h>
#include <cstdlib>

namespace ten {

template <class Unique, class T>
class thread_local {
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
            if (runtime::is_main_thread()) {
                std::atexit(thread_local::atexit);
            } else {
                static pthread_key_t key;
                pthread_key_create(&key, thread_local::placement_delete);
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
__thread typename thread_local<Unique, T>::storage_type thread_local<Unique, T>::storage;
template <class Unique, class T>
__thread bool thread_local<Unique, T>::allocated = false;

} // ten

#endif
