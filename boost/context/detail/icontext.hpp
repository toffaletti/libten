
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXTS_DETAIL_ICONTEXT_H
#define BOOST_CONTEXTS_DETAIL_ICONTEXT_H

#include <cstddef>

#include <boost/config.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/utility.hpp>

#include <boost/context/detail/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace contexts {
namespace detail {

class icontext : private noncopyable
{
private:
    std::size_t       use_count_;

public:
    typedef intrusive_ptr< icontext >   ptr_t;

    icontext() :
        use_count_( 0)
    {}

    virtual ~icontext() {}

    virtual bool unwind_requested() const = 0;

    virtual bool is_complete() const = 0;

    virtual bool is_started() const = 0;

    virtual bool is_resumed() const = 0;

    virtual bool is_running() const = 0;

    virtual void * start() = 0;

    virtual void * resume( void * vp) = 0;

    virtual void * suspend( void * vp) = 0;

    virtual void unwind_stack() = 0;

    virtual void exec() = 0;

    friend inline void intrusive_ptr_add_ref( icontext * p)
    { ++p->use_count_; }

    friend inline void intrusive_ptr_release( icontext * p)
    { if ( --p->use_count_ == 0) delete p; }
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_ICONTEXT_H
