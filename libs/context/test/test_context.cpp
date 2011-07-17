
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/utility.hpp>

#include <boost/context/all.hpp>

int value1 = 0;
std::string value2, value3;

class X : private boost::noncopyable
{
private:
    std::string     str_;

public:
    X( std::string const& str) :
        str_( str)
    {}

    ~X()
    { value3 = str_; }
};

boost::contexts::context<> gctx;

void fn0()
{}

void fn1( int i)
{ value1 = i; }

void fn2( std::string const& str)
{
    try
    { throw std::runtime_error( str); }
    catch ( std::runtime_error const& e)
    { value2 = e.what(); }
}

void fn3( std::string const& str)
{
    X x( str);
    gctx.suspend();
}

void fn4( std::string const& str1, std::string const& str2)
{
    value2 = str1;
    value3 = str2;
}

void fn5( double d)
{
    d += 3.45;
    std::cout << "d == " << d << std::endl;
}

void test_case_1()
{
    boost::contexts::context<> ctx1;
    boost::contexts::context<> ctx2(
        fn0,
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true);
    BOOST_CHECK( ! ctx1);
    BOOST_CHECK( ctx2);
    ctx1 = boost::move( ctx2);
    BOOST_CHECK( ctx1);
    BOOST_CHECK( ! ctx2);
}

void test_case_2()
{
    boost::contexts::context<> ctx(
        fn0,
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true);
    BOOST_CHECK( ctx);
    BOOST_CHECK( ctx.owns_stack() );
    ctx.release_stack();
    BOOST_CHECK( ctx);
    BOOST_CHECK( ! ctx.owns_stack() );
}

void test_case_3()
{
    boost::contexts::context<> ctx(
        fn0,
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true);
    BOOST_CHECK( ! ctx.is_complete() );
    ctx.resume();
    BOOST_CHECK( ctx.is_complete() );
}

void test_case_4()
{
    int i = 1;
    BOOST_CHECK_EQUAL( 0, value1);
    boost::contexts::protected_stack stack( boost::contexts::stack_helper::default_stacksize());
    boost::contexts::context<> ctx(
        fn1, i,
        boost::move( stack),
        true);
    BOOST_CHECK( ! ctx.is_complete() );
    ctx.resume();
    BOOST_CHECK( ctx.is_complete() );
    BOOST_CHECK_EQUAL( 1, value1);
}

void test_case_5()
{
    BOOST_CHECK_EQUAL( std::string(""), value2);
    boost::contexts::protected_stack stack( boost::contexts::stack_helper::default_stacksize());
    boost::contexts::context<> ctx(
        fn2, "abc",
        boost::move( stack),
        true, true);
    BOOST_CHECK( ! ctx.is_complete() );
    ctx.resume();
    BOOST_CHECK( ctx.is_complete() );
    BOOST_CHECK_EQUAL( std::string("abc"), value2);
}

void test_case_6()
{
    BOOST_CHECK_EQUAL( std::string(""), value3);
    gctx = boost::contexts::context<>(
        fn3, "abc",
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true);
    BOOST_CHECK( ! gctx.is_complete() );
    gctx.resume();
    BOOST_CHECK( ! gctx.is_complete() );
    BOOST_CHECK_EQUAL( std::string(""), value3);
    gctx.unwind_stack();
    BOOST_CHECK( gctx.is_complete() );
    BOOST_CHECK_EQUAL( std::string("abc"), value3);
}

void test_case_7()
{
    value1 = 0;
    value2 = "";
    value3 = "";
    BOOST_CHECK_EQUAL( 0, value1);
    BOOST_CHECK_EQUAL( std::string(""), value2);
    BOOST_CHECK_EQUAL( std::string(""), value3);

    boost::contexts::protected_stack stack( boost::contexts::stack_helper::default_stacksize());
    boost::contexts::context<> ctx1(
        fn4, "abc", "xyz",
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true, true);
    boost::contexts::context<> ctx2(
        fn1, 7,
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true, ctx1);

    BOOST_CHECK( ! ctx1.is_complete() );
    BOOST_CHECK( ! ctx2.is_complete() );
    ctx2.resume();
    BOOST_CHECK( ctx1.is_complete() );
    BOOST_CHECK( ctx2.is_complete() );

    BOOST_CHECK_EQUAL( 7, value1);
    BOOST_CHECK_EQUAL( "abc", value2);
    BOOST_CHECK_EQUAL( "xyz", value3);
}

void test_case_8()
{
    boost::contexts::context<> ctx(
        fn5, 7.34,
        boost::contexts::protected_stack( boost::contexts::stack_helper::default_stacksize()),
        true);
    BOOST_CHECK( ! ctx.is_complete() );
    ctx.resume();
    BOOST_CHECK( ctx.is_complete() );
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* [])
{
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Context: context test suite");

    test->add( BOOST_TEST_CASE( & test_case_1) );
    test->add( BOOST_TEST_CASE( & test_case_2) );
    test->add( BOOST_TEST_CASE( & test_case_3) );
    test->add( BOOST_TEST_CASE( & test_case_4) );
    test->add( BOOST_TEST_CASE( & test_case_5) );
    test->add( BOOST_TEST_CASE( & test_case_6) );
    test->add( BOOST_TEST_CASE( & test_case_7) );
    test->add( BOOST_TEST_CASE( & test_case_8) );

    return test;
}
