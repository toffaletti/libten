
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTEXTS_DETAIL_CONTEXT_OBJECT_H
#define BOOST_CONTEXTS_DETAIL_CONTEXT_OBJECT_H

#include <cstddef>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/move/move.hpp>
#include <boost/type_traits/remove_reference.hpp>

#include <boost/context/detail/config.hpp>
#include <boost/context/detail/context_base.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace contexts {
namespace detail {

template< typename Fn, typename StackT >
class context_object : public context_base< StackT >
{
private:
    Fn  fn_;

    context_object( context_object &);
    context_object & operator=( context_object const&);

public:
#ifndef BOOST_NO_RVALUE_REFERENCES
    context_object( Fn & fn, StackT && stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( fn)
    {}

    context_object( Fn & fn, StackT && stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( fn)
    {}

    context_object( Fn && fn, StackT && stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( static_cast< Fn && >( fn) )
    {}

    context_object( Fn && fn, StackT && stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( static_cast< Fn && >( fn) )
    {}
#else
    context_object( Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( fn)
    {}

    context_object( Fn fn, BOOST_RV_REF( StackT) stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( fn)
    {}

    context_object( BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( fn)
    {}

    context_object( BOOST_RV_REF( Fn) fn, BOOST_RV_REF( StackT) stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( fn)
    {}
#endif

    void exec()
    { fn_(); }
};

template< typename Fn, typename StackT >
class context_object< reference_wrapper< Fn >, StackT > : public context_base< StackT >
{
private:
    Fn  &   fn_;

    context_object( context_object &);
    context_object & operator=( context_object const&);

public:
    context_object( reference_wrapper< Fn > fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( fn)
    {}

    context_object( reference_wrapper< Fn > fn, BOOST_RV_REF( StackT) stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( fn)
    {}

    void exec()
    { fn_(); }
};

template< typename Fn, typename StackT >
class context_object< const reference_wrapper< Fn >, StackT > : public context_base< StackT >
{
private:
    Fn  &   fn_;

    context_object( context_object &);
    context_object & operator=( context_object const&);

public:
    context_object( const reference_wrapper< Fn > fn, BOOST_RV_REF( StackT) stack, bool do_unwind, bool do_return) :
        context_base< StackT >( boost::move( stack), do_unwind, do_return),
        fn_( fn)
    {}

    context_object( const reference_wrapper< Fn > fn, BOOST_RV_REF( StackT) stack, bool do_unwind, typename context_base< StackT >::ptr_t nxt) :
        context_base< StackT >( boost::move( stack), do_unwind, nxt),
        fn_( fn)
    {}

    void exec()
    { fn_(); }
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_CONTEXTS_DETAIL_CONTEXT_OBJECT_H
