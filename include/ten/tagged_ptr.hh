#ifndef TAGGED_PTR_HH
#define TAGGED_PTR_HH

#include "ten/logging.hh"
#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>

namespace ten {

inline uint16_t null_tagger() {
    return 1;
}

inline uint16_t tid_tagger() {
    static __thread pid_t tid(0);
    if (tid == 0) {
        tid = syscall(SYS_gettid);
        //assert((tid>>16) == 0)
    }
    return tid;
}

inline uint16_t count_tagger() {
    static __thread uint16_t count(0);
    return ++count;
}

template <typename T> uint16_t atomic_count_tagger() {
    static std::atomic<uint16_t> count(0);
    return ++count;
}

typedef uint16_t(* TaggerFuncT)();

template <typename T, TaggerFuncT Tagger=tid_tagger> struct tagged_ptr {
    uintptr_t _data;

    explicit tagged_ptr(T *p) {
        _data = reinterpret_cast<uintptr_t>(p);
        if (_data != 0 && tag() == 0) { // not tagged yet, don't tag nullptr
            // assert((data>>48) == 0)
            _data |= static_cast<uintptr_t>(Tagger())<<48;
            DVLOG(4) << "tagged " << p << " now " << (T *)_data;
        }
    }

    uint16_t tag() const {
        return _data >> 48;
    }

    T *get() const {
        return reinterpret_cast<T *>(_data);
    }

    T *ptr() const {
        return reinterpret_cast<T *>(_data & ((1ULL << 48) - 1));
    }

    T *operator ->() const {
        return ptr();
    }

    bool operator == (const T *rhs) const  {
        return get() == rhs;
    }

    bool operator == (const tagged_ptr &rhs) const  {
        return _data == rhs._data;
    }

    bool operator != (const T *rhs) const  {
        return get() != rhs;
    }

    bool operator != (const tagged_ptr &rhs) const  {
        return _data != rhs._data;
    }

    void free() {
        delete ptr();
    }
};

}

#endif
