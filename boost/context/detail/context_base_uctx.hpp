
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
#define BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H

extern "C" {
#include <ucontext.h>
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

namespace boost {
namespace contexts {
namespace detail {

struct ex_unwind_stack {};

template< typename Ctx >
void trampoline( void * vp)
{
    BOOST_ASSERT( vp);

    Ctx * ctx( static_cast< Ctx * >( vp) );

    try
    { ctx->exec(); }
    catch ( ex_unwind_stack const&)
    {
        ctx->flags_ |= Ctx::flag_complete;
        swapcontext( & ctx->ctx_callee_, & ctx->ctx_caller_);   
    }
    catch (...)
    { std::terminate(); }

    ctx->flags_ |= Ctx::flag_complete;

    // in order to return to the code invoked the context
    // nxt_->caller_ hast ot set to the first one
    if ( ctx->nxt_)
    {
        // FXIXME: swapping ucontext_t might not be correct
        // because of that copy-operation for ucontext_t is
        // not defined for POSIXi (== structure of ucontext_t
        // is not defined by POSIX)
        std::swap( ctx->nxt_->ctx_caller_, ctx->ctx_caller_);
        if ( 0 != ( ctx->nxt_->flags_ & Ctx::flag_do_return) )
            ctx->nxt_->ctx_callee_.uc_link = & ctx->nxt_->ctx_caller_;
        ctx->nxt_->flags_ |= Ctx::flag_running;
        ctx->nxt_->flags_ |= Ctx::flag_resumed;
    }
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
    friend void trampoline( void * vp);

    StackT                  stack_;
    ucontext_t              ctx_caller_;
    ucontext_t              ctx_callee_;
    ptr_t                   nxt_;
    short                   flags_;
    std::size_t             use_count_;

public:
    context_base( BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        stack_( boost::move( stack) ), ctx_caller_(), ctx_callee_(), nxt_(),
        flags_( do_unwind ? flag_force_unwind : flag_dont_force_unwind), use_count_( 0)
    {
        BOOST_ASSERT( stack_);

        void * base( stack_.address() );
        BOOST_ASSERT( base);

        std::memset( & ctx_caller_, 0, sizeof( ctx_caller_) );
        std::memset( & ctx_callee_, 0, sizeof( ctx_callee_) );
        getcontext( & ctx_callee_);
        ctx_callee_.uc_stack.ss_size = stack_.size();
        ctx_callee_.uc_stack.ss_sp = static_cast< char * >( base) - stack_.size();

        if ( do_return)
        {
            flags_ |= flag_do_return;
            ctx_callee_.uc_link = & ctx_caller_;
        }

        typedef void fn_type( void *);
        fn_type * fn_ptr( trampoline< context_base< StackT > >);

        makecontext( & ctx_callee_, ( void(*)() )( fn_ptr), 1, this);
    }

    context_base( BOOST_RV_REF( StackT) stack, bool do_unwind, ptr_t nxt) :
        stack_( boost::move( stack) ), ctx_caller_(), ctx_callee_(), nxt_( nxt),
        flags_( do_unwind ? flag_force_unwind : flag_dont_force_unwind), use_count_( 0)
    {
        BOOST_ASSERT( stack_);
        BOOST_ASSERT( ! nxt->is_complete() );

        void * base( stack_.address() );
        BOOST_ASSERT( base);

        std::memset( & ctx_caller_, 0, sizeof( ctx_caller_) );
        std::memset( & ctx_callee_, 0, sizeof( ctx_callee_) );
        getcontext( & ctx_callee_);
        ctx_callee_.uc_stack.ss_size = stack_.size();
        ctx_callee_.uc_stack.ss_sp = static_cast< char * >( base) - stack_.size();
        ctx_callee_.uc_link = & nxt->ctx_callee_;

        typedef void fn_type( void *);
        fn_type * fn_ptr( trampoline< context_base< StackT > >);

        makecontext( & ctx_callee_, ( void(*)() )( fn_ptr), 1, this);
    }

    virtual ~context_base()
    {
        if ( owns_stack() && ! is_complete()
                && ( 0 != ( flags_ & flag_resumed) )
                && ( 0 != ( flags_ & flag_force_unwind) ) )
            unwind_stack();
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
        swapcontext( & ctx_caller_, & ctx_callee_);
    }

    void suspend()
    {
        BOOST_ASSERT( owns_stack() );
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( is_running() );

        flags_ &= ~flag_running;
        swapcontext( & ctx_callee_, & ctx_caller_);
        if ( 0 != ( flags_ & flag_unwind_stack) )
            throw ex_unwind_stack();
    }

    void unwind_stack()
    {
        BOOST_ASSERT( owns_stack() );
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_unwind_stack;
        swapcontext( & ctx_caller_, & ctx_callee_);
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

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
