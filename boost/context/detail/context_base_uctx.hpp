
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
#include <boost/utility.hpp>

#include <boost/context/detail/config.hpp>
#include <boost/context/detail/icontext.hpp>
#include <boost/context/flags.hpp>

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
        // not defined for POSIX (== structure of ucontext_t
        // is not defined by POSIX)
		Ctx * nxt( dynamic_cast< Ctx * >( ctx->nxt_.get() ) );
		BOOST_ASSERT( nxt);
        std::swap( nxt->ctx_caller_, ctx->ctx_caller_);
        if ( 0 != ( nxt->flags_ & Ctx::flag_do_return) )
            nxt->ctx_callee_.uc_link = & nxt->ctx_caller_;
        nxt->flags_ |= Ctx::flag_running;
        nxt->flags_ |= Ctx::flag_started;
    }
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
    friend void trampoline( void * vp);

    Allocator               alloc_;
    ucontext_t              ctx_caller_;
    ucontext_t              ctx_callee_;
    ptr_t                   nxt_;
    short                   flags_;
	void				*	vp_;

public:
    context_base( Allocator const& alloc, std::size_t size, flag_unwind_t do_unwind, flag_return_t do_return) :
        alloc_( alloc), ctx_caller_(), ctx_callee_(), nxt_(),
        flags_( stack_unwind == do_unwind ? flag_force_unwind : flag_dont_force_unwind), vp_( 0)
    {
		void * base( alloc_.allocate( size) );
        BOOST_ASSERT( base);

        std::memset( & ctx_caller_, 0, sizeof( ctx_caller_) );
        std::memset( & ctx_callee_, 0, sizeof( ctx_callee_) );
        getcontext( & ctx_callee_);
        ctx_callee_.uc_stack.ss_size = size;
        ctx_callee_.uc_stack.ss_sp = static_cast< char * >( base) - size;

        if ( return_to_caller == do_return)
        {
            flags_ |= flag_do_return;
            ctx_callee_.uc_link = & ctx_caller_;
        }

        typedef void fn_type( void *);
        fn_type * fn_ptr( trampoline< context_base< Allocator > >);

        makecontext( & ctx_callee_, ( void(*)() )( fn_ptr), 1, this);
    }

    context_base( Allocator const& alloc, std::size_t size, flag_unwind_t do_unwind, ptr_t nxt) :
        alloc_( alloc), ctx_caller_(), ctx_callee_(), nxt_( nxt),
        flags_( stack_unwind == do_unwind ? flag_force_unwind : flag_dont_force_unwind), vp_( 0)
    {
        BOOST_ASSERT( ! nxt_->is_complete() );

		void * base( alloc_.allocate( size) );
        BOOST_ASSERT( base);

        std::memset( & ctx_caller_, 0, sizeof( ctx_caller_) );
        std::memset( & ctx_callee_, 0, sizeof( ctx_callee_) );
        getcontext( & ctx_callee_);
        ctx_callee_.uc_stack.ss_size = size;
        ctx_callee_.uc_stack.ss_sp = static_cast< char * >( base) - size;
        ctx_callee_.uc_link = & dynamic_pointer_cast< context_base< Allocator > >( nxt_)->ctx_callee_;

        typedef void fn_type( void *);
        fn_type * fn_ptr( trampoline< context_base< Allocator > >);

        makecontext( & ctx_callee_, ( void(*)() )( fn_ptr), 1, this);
    }

    virtual ~context_base()
    {
        if ( ! is_complete()
                && ( is_started() || is_resumed() )
                && ( unwind_requested() ) )
            unwind_stack();
		void * base( 
            static_cast< char * >( ctx_callee_.uc_stack.ss_sp) +
			ctx_callee_.uc_stack.ss_size);
		alloc_.deallocate(
			base, ctx_callee_.uc_stack.ss_size);
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
        swapcontext( & ctx_caller_, & ctx_callee_);

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
        swapcontext( & ctx_caller_, & ctx_callee_);

		return vp_;
    }

    void * suspend( void * vp)
    {
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( is_running() );

        flags_ &= ~flag_running;
		vp_ = vp;
        swapcontext( & ctx_callee_, & ctx_caller_);
        if ( 0 != ( flags_ & flag_unwind_stack) )
            throw ex_unwind_stack();

		return vp_;
    }

    void unwind_stack()
    {
        BOOST_ASSERT( ! is_complete() );
        BOOST_ASSERT( ! is_running() );

        flags_ |= flag_unwind_stack;
        swapcontext( & ctx_caller_, & ctx_callee_);
        flags_ &= ~flag_unwind_stack;
        BOOST_ASSERT( is_complete() );
    }
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_CONTEXT_BASE_H
