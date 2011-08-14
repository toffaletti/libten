#ifndef ATOMIC_HH
#define ATOMIC_HH

#include <ext/atomicity.h>
#include <ostream>

namespace fw {

#if defined(__GLIBCXX__) // g++ 3.4+
using __gnu_cxx::__atomic_add;
using __gnu_cxx::__exchange_and_add;
#endif

//! word size atomic counter
class atomic_count {
public:
    explicit atomic_count( int v ) : value_( v ) {}

    int operator++() {
        return __exchange_and_add( &value_, +1 ) + 1;
    }

    int operator--() {
        return __exchange_and_add( &value_, -1 ) - 1;
    }

    operator int() const {
        return __exchange_and_add( &value_, 0 );
    }

private:
    atomic_count(atomic_count const &);
    atomic_count & operator=(atomic_count const &);

    mutable _Atomic_word value_;
};

// http://gcc.gnu.org/onlinedocs/gcc-4.5.0/gcc/Atomic-Builtins.html

//! atomic bitwise operations
template <typename T> class atomic_bits {
public:
    explicit atomic_bits(T v) : value_(v) {}

    T operator & (const T &b) {
        return __sync_fetch_and_add(&value_, 0) & b;
    }

    T operator ^ (const T &b) {
        return __sync_fetch_and_add(&value_, 0) ^ b;
    }

    T operator &= (const T &b) {
        return __sync_and_and_fetch(&value_, b);
    }

    T operator |= (const T &b) {
        return __sync_or_and_fetch(&value_, b);
    }

    T operator ^= (const T &b) {
        return __sync_xor_and_fetch(&value_, b);
    }

    friend std::ostream &operator << (std::ostream &o, const atomic_bits<T> &a) {
        o << __sync_fetch_and_add(&a.value_, 0);
        return o;
    }
private:
    atomic_bits(atomic_bits const &);
    atomic_bits& operator=(atomic_bits const &);

    mutable T value_;
};

} // end namesapce fw

#endif // ATOMIC_HH

