#ifndef LIBTEN_THREAD_LOCAL_HH
#define LIBTEN_THREAD_LOCAL_HH

#include <type_traits>
#include <pthread.h>

namespace ten {

template<typename T> static void placement_delete(void *v) { ((T *)v)->~T(); }

template <typename T, int Tag=0, typename ...Args> T *thread_local_ptr(Args... args) {
    typedef typename std::aligned_storage<sizeof(T),
            std::alignment_of<T>::value>::type storage_type;

    static __thread storage_type storage;
    static __thread bool allocated = false;
    if (!allocated) {
        new (&storage) T(args...);
        allocated = true;
        static pthread_key_t key;
        pthread_key_create(&key, placement_delete<T>);
        pthread_setspecific(key, &storage);
    }
    return (T*)(&storage);
};

} // ten

#endif
