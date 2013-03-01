#ifndef LIBTEN_THREAD_LOCAL_HH
#define LIBTEN_THREAD_LOCAL_HH

#include <type_traits>
#include <memory>
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
    struct holder {
        bool *allocated;
        void *ptr;
    };

    typedef typename std::aligned_storage<sizeof(T),
            std::alignment_of<T>::value>::type storage_type;

    static void thread_cleanup(void *v) {
        std::unique_ptr<holder> h{static_cast<holder *>(v)};
        placement_delete(h.get()); 
    }

    static void placement_delete(holder *h) {
        static_cast<T*>(h->ptr)->~T();
        *(h->allocated) = false;
    }

    // only used by main thread
    // other threads will use heap allocated holder
    // and do cleanup in thread_cleanup
    struct holder _main_holder = {};
public:
    ~thread_cached() {
        if (_main_holder.allocated && *_main_holder.allocated) {
            placement_delete(&_main_holder); 
        }
    }

    T *get() {
        static __thread bool allocated = false;
        static __thread storage_type storage;
        if (!allocated) {
            new (&storage) T();
            allocated = true;
            if (is_main_thread()) {
                _main_holder.allocated = &allocated;
                _main_holder.ptr = &storage;
            } else {
                static pthread_key_t key;
                pthread_key_create(&key, thread_cached::thread_cleanup);
                std::unique_ptr<holder> hp{new holder{&allocated, &storage}};
                pthread_setspecific(key, hp.release());
            }
        }
        return reinterpret_cast<T*>(&storage);
    }

    T *operator-> () {
        return get();
    }
};

} // ten

#endif
