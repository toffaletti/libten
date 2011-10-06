
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
//  This code was influenced by boost.coroutine writteen by Giovanni P. Deretta.

#ifndef BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
#define BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H

extern "C" {
#include <windows.h>
}

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/utility.hpp>

#include <boost/context/detail/config.hpp>
#include <boost/context/detail/icontext.hpp>
#include <boost/context/flags.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

# if defined(BOOST_MSVC)
# pragma warning(push)
# pragma warning(disable:4800)
# endif

namespace boost {
namespace contexts {
namespace detail {

// Note: for Windows XP + SP 2/3 the following
// condition is true but function IsThreadAFiber()
// is not provided

//#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
//  inline
//  bool is_a_fiber()
//  { return TRUE == ::IsThreadAFiber(); }
//#else
inline
bool is_a_fiber()
{
    LPVOID current( ::GetCurrentFiber() );
    return 0 != current && reinterpret_cast< LPVOID >( 0x1E00) != current;
}
//#endif

struct ex_unwind_stack {};

template< typename Ctx >
void CALLBACK trampoline( void * vp)
{
    BOOST_ASSERT( vp);

    Ctx * ctx( static_cast< Ctx * >( vp) );

    try
    {
        ctx->exec();
        ctx->flags_ |= Ctx::flag_complete;
		Ctx * nxt( dynamic_cast< Ctx * >( ctx->nxt_.get() ) );
		BOOST_ASSERT( nxt);
        if ( nxt)
        {
            std::swap( nxt->ctx_caller_, ctx->ctx_caller_);
            nxt->flags_ |= Ctx::flag_running;
            nxt->flags_ |= Ctx::flag_started;
            ::SwitchToFiber( nxt->ctx_callee_);
        }
        if ( 0 != ( ctx->flags_ & Ctx::flag_do_return) ) ::SwitchToFiber( ctx->ctx_caller_);
    }
    catch ( ex_unwind_stack const&)
    {
        ctx->flags_ |= Ctx::flag_complete;
        SwitchToFiber( ctx->ctx_caller_);
    }
    catch (...)
    { std::terminate(); }
}

template< typename Allocator >
class context_base : public icontext
{
private:
    enum flag_t
    {
        flag_started                = 1 << 1,
        flag_resumed                = 1 << 2,
        flag_running                = 1 << 3,
        flag_complete               = 1 << 4,
        flag_unwind_stack           = 1 << 5,
        flag_force_unwind           = 1 << 6,
        flag_dont_force_unwind      = 1 << 7,
        flag_do_return              = 1 << 8,
    };

    template< typename T >
    friend void CALLBACK trampoline( void * vp);

    LPVOID              ctx_caller_;
    LPVOID              ctx_callee_;
    ptr_t               nxt_;
    short               flags_;
	void			*	vp_;

public:
    context_base( Allocator const& alloc, std::size_t size, flag_unwind_t do_unwind, flag_return_t do_return) :
        ctx_caller_( 0), ctx_callee_( 0), nxt_(),
        flags_( stack_unwind == do_unwind ? flag_force_unwind : flag_dont_force_unwind), vp_( 0)
    {
        if ( return_to_caller == do_return) flags_ |= flag_do_return;

        ctx_callee_ = ::CreateFiber(
            size,
            trampoline< context_base< Allocator > >,
            static_cast< LPVOID >( this) );
        BOOST_ASSERT( ctx_callee_);
    }

    context_base( Allocator const& alloc, std::size_t size, flag_unwind_t do_unwind, ptr_t nxt) :
        ctx_caller_( 0), ctx_callee_( 0), nxt_( nxt),
        flags_( stack_unwind == do_unwind ? flag_force_unwind : flag_dont_force_unwind), vp_( 0)
    {
        BOOST_ASSERT( ! nxt_->is_complete() );

        ctx_callee_ = ::CreateFiber(
            size,
            trampoline< context_base< Allocator > >,
            static_cast< LPVOID >( this) );
        BOOST_ASSERT( ctx_callee_);
    }

    virtual ~context_base()
    {
        if ( ! is_complete()
                && ( is_started() || is_resumed() )
                && ( unwind_requested() ) )
            unwind_stack();
        if ( ctx_callee_) DeleteFiber( ctx_callee_);
    }

    bool unwind_requested() const
    { return 0 != ( flags_ & flag_force_unwind); }

    bool is_complete() const
    { return 0 != ( flags_ & flag_complete); }

    bool is_started() const
    { return 0 != ( flags_ & flag_started); }

    bool is_resumed() const
    { return 0 != ( flags_ & flag_started); }

    bool is_running() const
    { return 0 != ( flags_ & flag_running); }

    void * start()
    {
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_started() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_started;
        flags_ |= flag_running;
        if ( ! is_a_fiber() )
        {
            ctx_caller_ = ConvertThreadToFiber( 0);
            BOOST_ASSERT( ctx_caller_);

            SwitchToFiber( ctx_callee_);

            BOOL result = ConvertFiberToThread();
            BOOST_ASSERT( result);
            ctx_caller_ = 0;
        }
        else
        {
            bool is_main = 0 == ctx_caller_;
            if ( is_main) ctx_caller_ = GetCurrentFiber();

            SwitchToFiber( ctx_callee_);

            if ( is_main) ctx_caller_ = 0;
        }

		return vp_;
    }

    void * resume( void * vp)
    {
        BOOST_ASSERT( is_started() );
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_resumed;
        flags_ |= flag_running;
		vp_ = vp;
        if ( ! is_a_fiber() )
        {
            ctx_caller_ = ConvertThreadToFiber( 0);
            BOOST_ASSERT( ctx_caller_);

            SwitchToFiber( ctx_callee_);

            BOOL result = ConvertFiberToThread();
            BOOST_ASSERT( result);
            ctx_caller_ = 0;
        }
        else
        {
            bool is_main = 0 == ctx_caller_;
            if ( is_main) ctx_caller_ = GetCurrentFiber();

            SwitchToFiber( ctx_callee_);

            if ( is_main) ctx_caller_ = 0;
        }

		return vp_;
    }

    void * suspend( void * vp)
    {
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( is_running() );
        BOOST_ASSERT( ctx_caller_); 

        flags_ &= ~flag_running;
		vp_ = vp;
        SwitchToFiber( ctx_caller_);
        if ( 0 != ( flags_ & flag_unwind_stack) )
            throw ex_unwind_stack();

		return vp_;
    }

    void unwind_stack()
    {
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_unwind_stack;
        if ( ! is_a_fiber() )
        {
            ctx_caller_ = ConvertThreadToFiber( 0);
            BOOST_ASSERT( ctx_caller_);

            SwitchToFiber( ctx_callee_);

            BOOL result = ConvertFiberToThread();
            BOOST_ASSERT( result);
            ctx_caller_ = 0;
        }
        else
        {
            bool is_main = 0 == ctx_caller_;
            if ( is_main) ctx_caller_ = GetCurrentFiber();

            SwitchToFiber( ctx_callee_);

            if ( is_main) ctx_caller_ = 0;
        }
        flags_ &= ~flag_unwind_stack;
        BOOST_ASSERT( is_complete() );
    }
};

}}}

# if defined(BOOST_MSVC)
# pragma warning(pop)
# endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
