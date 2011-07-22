#ifndef ATOMIC_HH
#define ATOMIC_HH

#include <ext/atomicity.h>

#if defined(__GLIBCXX__) // g++ 3.4+
using __gnu_cxx::__atomic_add;
using __gnu_cxx::__exchange_and_add;
#endif

class atomic_count
{
public:
    explicit atomic_count( int v ) : value_( v ) {}

    int operator++()
    {
        return __exchange_and_add( &value_, +1 ) + 1;
    }

    int operator--()
    {
        return __exchange_and_add( &value_, -1 ) - 1;
    }

    operator int() const
    {
        return __exchange_and_add( &value_, 0 );
    }

private:
    atomic_count(atomic_count const &);
    atomic_count & operator=(atomic_count const &);

    mutable _Atomic_word value_;
};

#endif // ATOMIC_HH

