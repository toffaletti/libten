
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXTS_CONTEXT_H
#define BOOST_CONTEXTS_CONTEXT_H

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/config.hpp>
#include <boost/move/move.hpp>
#include <boost/preprocessor/repetition.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/context/detail/context_base.hpp>
#include <boost/context/detail/context_object.hpp>
#include <boost/context/protected_stack.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace contexts {

template< typename StackT = protected_stack >
class context
{
private:
    typedef typename detail::context_base< StackT >::ptr_t  base_ptr_t;

    base_ptr_t  impl_;

    BOOST_MOVABLE_BUT_NOT_COPYABLE( context);

#ifndef BOOST_NO_RVALUE_REFERENCES
    static base_ptr_t make_context_(
        void( * fn)(), StackT && stack, bool do_unwind, bool do_return)
    {
        return base_ptr_t(
            new detail::context_object< void(*)(), StackT >(
                fn, boost::move( stack), do_unwind, do_return) );
    }

    static base_ptr_t make_context_(
        void( * fn)(), StackT && stack, bool do_unwind, context & nxt)
    {
        BOOST_ASSERT( nxt);
        return base_ptr_t(
            new detail::context_object< void(*)(), StackT >(
                fn, boost::move( stack), do_unwind, nxt.impl_) );
    }

    template< typename Fn >
    static base_ptr_t make_context_(
        Fn && fn, StackT && stack, bool do_unwind, bool do_return)
    {
        return base_ptr_t(
            new detail::context_object< typename remove_reference< Fn >::type, StackT >(
                fn, boost::move( stack), do_unwind, do_return) );
    }

    template< typename Fn >
    static base_ptr_t make_context_(
        Fn && fn, StackT && stack, bool do_unwind, context & nxt)
    {
        BOOST_ASSERT( nxt);
        return base_ptr_t(
            new detail::context_object< typename remove_reference< Fn >::type, StackT >(
                fn, boost::move( stack), do_unwind, nxt.impl_) );
    }
#else
    template< typename Fn >
    static base_ptr_t make_context_(
        Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return)
    {
        return base_ptr_t(
            new detail::context_object< Fn, StackT >(
                fn, boost::move( stack), do_unwind, do_return) );
    }

    template< typename Fn >
    static base_ptr_t make_context_(
        Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, context & nxt)
    {
        BOOST_ASSERT( nxt);
        return base_ptr_t(
            new detail::context_object< Fn, StackT >(
                fn, boost::move( stack), do_unwind, nxt.impl_) );
    }

    template< typename Fn >
    static base_ptr_t make_context_(
        BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return)
    {
        return base_ptr_t(
            new detail::context_object< Fn, StackT >(
                fn, boost::move( stack), do_unwind, do_return) );
    }

    template< typename Fn >
    static base_ptr_t make_context_(
        BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, context & nxt)
    {
        BOOST_ASSERT( nxt);
        return base_ptr_t(
            new detail::context_object< Fn, StackT >(
                fn, boost::move( stack), do_unwind, nxt.impl_) );
    }
#endif

public:
    typedef void ( * unspecified_bool_type)( context ***);

    static void unspecified_bool( context ***) {}

    context() :
        impl_()
    {}

#ifndef BOOST_NO_RVALUE_REFERENCES
# ifdef BOOST_MSVC
    template< typename Fn >
    context( Fn & fn, StackT && stack, bool do_unwind, bool do_return = true) :
        impl_( make_context_( static_cast< Fn & >( fn), boost::move( stack), do_unwind, do_return) )
    {}

    template< typename Fn >
    context( Fn & fn, StackT && stack, bool do_unwind, context & nxt) :
        impl_( make_context_( static_cast< Fn & >( fn), boost::move( stack), do_unwind, nxt) )
    {}
# endif
    template< typename Fn >
    context( Fn && fn, StackT && stack, bool do_unwind, bool do_return = true) :
        impl_( make_context_( static_cast< Fn && >( fn), boost::move( stack), do_unwind, do_return) )
    {}

    template< typename Fn >
    context( Fn && fn, StackT && stack, bool do_unwind, context & nxt) :
        impl_( make_context_( static_cast< Fn && >( fn), boost::move( stack), do_unwind, nxt) )
    {}
#else
    template< typename Fn >
    context( Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return = true) :
        impl_( make_context_( fn, boost::move( stack), do_unwind, do_return) )
    {}

    template< typename Fn >
    context( Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, context & nxt) :
        impl_( make_context_( fn, boost::move( stack), do_unwind, nxt) )
    {}

    template< typename Fn >
    context( BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return = true) :
        impl_( make_context_( fn, boost::move( stack), do_unwind, do_return) )
    {}

    template< typename Fn >
    context( BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, context & nxt) :
        impl_( make_context_( fn, boost::move( stack), do_unwind, nxt) )
    {}
#endif

#define BOOST_CONTEXT_ARG(z, n, unused) BOOST_PP_CAT(A, n) BOOST_PP_CAT(a, n)

#define BOOST_CONTEXT_ARGS(n) BOOST_PP_ENUM(n, BOOST_CONTEXT_ARG, ~)

#define BOOST_CONTEXT_CTOR(z, n, unused) \
    template< typename Fn, BOOST_PP_ENUM_PARAMS(n, typename A) > \
    context( Fn fn, BOOST_CONTEXT_ARGS(n), BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return = true) : \
        impl_( \
            make_context_( \
                boost::bind( boost::type< void >(), fn, BOOST_PP_ENUM_PARAMS(n, a) ), \
                boost::move( stack), do_unwind, do_return) ) \
    {} \
    \
    template< typename Fn, BOOST_PP_ENUM_PARAMS(n, typename A) > \
    context( Fn fn, BOOST_CONTEXT_ARGS(n), BOOST_RV_REF( StackT) stack, bool do_unwind, context & nxt) : \
        impl_( \
            make_context_( \
                boost::bind( boost::type< void >(), fn, BOOST_PP_ENUM_PARAMS(n, a) ), \
                boost::move( stack), do_unwind, nxt) ) \
    {} \

#ifndef BOOST_CONTEXT_ARITY
#define BOOST_CONTEXT_ARITY 10
#endif

BOOST_PP_REPEAT_FROM_TO( 1, BOOST_CONTEXT_ARITY, BOOST_CONTEXT_CTOR, ~)

#undef BOOST_CONTEXT_CTOR
#undef BOOST_CONTEXT_ARGS
#undef BOOST_CONTEXT_ARG

    context( BOOST_RV_REF( context) other) :
        impl_()
    { swap( other); }

    context & operator=( BOOST_RV_REF( context) other)
    {
        if ( this == & other) return * this;
        context tmp( boost::move( other) );
        swap( tmp);
        return * this;
    }

    operator unspecified_bool_type() const
    { return impl_ ? unspecified_bool : 0; }

    bool operator!() const
    { return ! impl_; }

    void swap( context & other)
    { impl_.swap( other.impl_); }

    void resume()
    {
        BOOST_ASSERT( impl_);
        return impl_->resume();
    }

    void suspend()
    {
        BOOST_ASSERT( impl_);
        return impl_->suspend();
    }

    void unwind_stack()
    {
        BOOST_ASSERT( impl_);
        impl_->unwind_stack();
    }

    bool is_complete() const
    {
        BOOST_ASSERT( impl_);
        return impl_->is_complete();
    }

    bool owns_stack() const
    {
        BOOST_ASSERT( impl_);
        return impl_->owns_stack();
    }

    StackT release_stack()
    {
        BOOST_ASSERT( impl_);
        return impl_->release_stack();
    }
};

template< typename StackT >
void swap( context< StackT > & l, context< StackT > & r)
{ l.swap( r); }

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_CONTEXT_H
