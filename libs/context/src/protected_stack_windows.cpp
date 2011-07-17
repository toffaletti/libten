
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/protected_stack.hpp>

extern "C" {
#include <windows.h>
}

#include <stdexcept>

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/format.hpp>

#include <boost/context/stack_helper.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

# if defined(BOOST_MSVC)
# pragma warning(push)
# pragma warning(disable:4244 4267)
# endif

namespace boost {
namespace contexts {

protected_stack::protected_stack() :
    size_( 0), size__( 0), address_( 0)
{}

protected_stack::protected_stack( std::size_t size) :
    size_( size), size__( 0), address_( 0)
{
    if ( stack_helper::minimal_stacksize() > size_)
        throw std::invalid_argument(
            boost::str( boost::format("invalid stack size: must be at least %d bytes")
                % stack_helper::minimal_stacksize() ) );

    if ( ! stack_helper::unlimited_stacksize() && ( stack_helper::maximal_stacksize() < size_) )
        throw std::invalid_argument(
            boost::str( boost::format("invalid stack size: must not be larger than %d bytes")
                % stack_helper::maximal_stacksize() ) );

    const std::size_t pages( stack_helper::page_count( size_) + 1); // add +1 for guard page
    size__ = pages * stack_helper::pagesize();

#ifndef BOOST_CONTEXT_FIBER
    void * limit = ::VirtualAlloc( 0, size__, MEM_COMMIT, PAGE_READWRITE);
    if ( ! limit) throw std::bad_alloc();

    DWORD old_options;
    const BOOL result = ::VirtualProtect(
        limit, stack_helper::pagesize(), PAGE_READWRITE | PAGE_GUARD /*PAGE_NOACCESS*/, & old_options);
    BOOST_ASSERT( FALSE != result);

    address_ = static_cast< char * >( limit) + size__;
#endif
}

protected_stack::~protected_stack()
{
    if ( address_)
    {
        BOOST_ASSERT( 0 < size_ && 0 < size__);
        void * limit = static_cast< char * >( address_) - size__;
        ::VirtualFree( limit, 0, MEM_RELEASE);
    }
}

protected_stack::protected_stack( BOOST_RV_REF( protected_stack) other) :
    size_( 0), size__( 0), address_( 0)
{ swap( other); }

protected_stack &
protected_stack::operator=( BOOST_RV_REF( protected_stack) other)
{
    protected_stack tmp( boost::move( other) );
    swap( tmp);
    return * this;
}

void *
protected_stack::address() const
{ return address_; }

std::size_t
protected_stack::size() const
{ return size_; }

void
protected_stack::swap( protected_stack & other)
{
    std::swap( size_, other.size_);
    std::swap( size__, other.size__);
    std::swap( address_, other.address_);
}

bool
protected_stack::operator!() const
{
#ifdef BOOST_CONTEXT_FIBER
    return 0 == size_;
#else
    return 0 == address_;
#endif
}

protected_stack::operator unspecified_bool_type() const
{
#ifdef BOOST_CONTEXT_FIBER
    return 0 == size_ ? 0 : unspecified_bool;
#else
    return 0 == address_ ? 0 : unspecified_bool;
#endif
}

}}

# if defined(BOOST_MSVC)
# pragma warning(pop)
# endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
