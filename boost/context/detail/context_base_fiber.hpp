
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
#include <boost/move/move.hpp>
#include <boost/utility.hpp>

#include <boost/context/detail/config.hpp>
#include <boost/context/protected_stack.hpp>

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
        if ( ctx->nxt_)
        {
            std::swap( ctx->nxt_->ctx_caller_, ctx->ctx_caller_);
            ctx->nxt_->flags_ |= Ctx::flag_running;
            ctx->nxt_->flags_ |= Ctx::flag_resumed;
            ::SwitchToFiber( ctx->nxt_->ctx_callee_);
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

template< typename StackT >
class context_base : private noncopyable
{
public:
    typedef intrusive_ptr< context_base >   ptr_t;

private:
    enum flag_t
    {
        flag_resumed                = 1 << 1,
        flag_running                = 1 << 2,
        flag_complete               = 1 << 3,
        flag_unwind_stack           = 1 << 4,
        flag_force_unwind           = 1 << 5,
        flag_dont_force_unwind      = 1 << 6,
        flag_do_return              = 1 << 7,
    };

    template< typename T >
    friend void CALLBACK trampoline( void * vp);

    StackT              stack_;
    LPVOID              ctx_caller_;
    LPVOID              ctx_callee_;
    ptr_t               nxt_;
    short               flags_;
    std::size_t         use_count_;

public:
    context_base( BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        stack_( boost::move( stack) ), ctx_caller_( 0), ctx_callee_( 0), nxt_(),
        flags_( do_unwind ? flag_force_unwind : flag_dont_force_unwind), use_count_( 0)
    {
        BOOST_ASSERT( stack_);

        if ( do_return) flags_ |= flag_do_return;

        ctx_callee_ = ::CreateFiber(
            stack_.size(),
            trampoline< context_base< StackT > >,
            static_cast< LPVOID >( this) );
        BOOST_ASSERT( ctx_callee_);
    }

    context_base( BOOST_RV_REF( StackT) stack, bool do_unwind, ptr_t nxt) :
        stack_( boost::move( stack) ), ctx_caller_( 0), ctx_callee_( 0), nxt_( nxt),
        flags_( do_unwind ? flag_force_unwind : flag_dont_force_unwind), use_count_( 0)
    {
        BOOST_ASSERT( stack_);
        BOOST_ASSERT( ! nxt_->is_complete() );

        ctx_callee_ = ::CreateFiber(
            stack_.size(),
            trampoline< context_base< StackT > >,
            static_cast< LPVOID >( this) );
        BOOST_ASSERT( ctx_callee_);
    }

    virtual ~context_base()
    {
        if ( owns_stack() && ! is_complete()
                && ( 0 != ( flags_ & flag_resumed) )
                && ( 0 != ( flags_ & flag_force_unwind) ) )
            unwind_stack();
        if ( ctx_callee_) DeleteFiber( ctx_callee_);
    }

    bool is_complete() const
    { return 0 != ( flags_ & flag_complete); }

    bool is_running() const
    { return 0 != ( flags_ & flag_running); }

    bool owns_stack() const
    { return static_cast< bool >( stack_); }

    StackT release_stack()
    { return boost::move( stack_); }

    void resume()
    {
        BOOST_ASSERT( owns_stack() );
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_resumed;
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
    }

    void suspend()
    {
        BOOST_ASSERT( owns_stack() );
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( is_running() );
        BOOST_ASSERT( ctx_caller_); 

        flags_ &= ~flag_running;
        SwitchToFiber( ctx_caller_);
        if ( 0 != ( flags_ & flag_unwind_stack) )
            throw ex_unwind_stack();
    }

    void unwind_stack()
    {
        BOOST_ASSERT( owns_stack() );
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

    virtual void exec() = 0;

    friend inline void intrusive_ptr_add_ref( context_base * p)
    { ++p->use_count_; }

    friend inline void intrusive_ptr_release( context_base * p)
    { if ( --p->use_count_ == 0) delete p; }
};

}}}

# if defined(BOOST_MSVC)
# pragma warning(pop)
# endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
