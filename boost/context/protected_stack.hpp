
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXTS_PROTECTED_STACK_H
#define BOOST_CONTEXTS_PROTECTED_STACK_H

#include <cstddef>

#include <boost/config.hpp>
#include <boost/move/move.hpp>

#include <boost/context/detail/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace contexts {

class BOOST_CONTEXT_DECL protected_stack
{
private:
    BOOST_MOVABLE_BUT_NOT_COPYABLE( protected_stack);

    std::size_t     size_;
    std::size_t     size__;
    void        *   address_;

public:
    typedef void ( * unspecified_bool_type)( protected_stack ***);

    static void unspecified_bool( protected_stack ***) {}

    protected_stack();

    explicit protected_stack( std::size_t);

    ~protected_stack();

    protected_stack( BOOST_RV_REF( protected_stack) other);

    protected_stack & operator=( BOOST_RV_REF( protected_stack) other);

    void * address() const;

    std::size_t size() const;

    void swap( protected_stack & other);

    bool operator!() const;

    operator unspecified_bool_type() const;
};

inline
void swap( protected_stack & l, protected_stack & r)
{ l.swap( r); }

}

}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_PROTECTED_STACK_H
