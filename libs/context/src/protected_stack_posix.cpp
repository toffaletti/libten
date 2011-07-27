
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_CONTEXT_SOURCE

#include <boost/context/protected_stack.hpp>

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

#include <stdexcept>

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/format.hpp>

#include <boost/context/stack_helper.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

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

    const int fd( ::open("/dev/zero", O_RDONLY) );
    BOOST_ASSERT( -1 != fd);
    void * limit =
        ::mmap( 0, size__, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    ::close( fd);
    if ( ! limit) throw std::bad_alloc();

    const int result( ::mprotect( limit, stack_helper::pagesize(), PROT_NONE) );
    BOOST_ASSERT( 0 == result);
    address_ = static_cast< char * >( limit) + size__;
}

protected_stack::~protected_stack()
{
    if ( address_)
    {
        BOOST_ASSERT( 0 < size_ && 0 < size__);
        void * limit = static_cast< char * >( address_) - size__;
        ::munmap( limit, size__);
    }
}
#if 0
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
#endif
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
{ return 0 == address_; }

protected_stack::operator unspecified_bool_type() const
{ return 0 == address_ ? 0 : unspecified_bool; }

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

