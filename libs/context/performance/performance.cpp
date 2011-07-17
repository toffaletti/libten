
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/context/all.hpp>
#include <boost/program_options.hpp>

#include "bind_processor.hpp"
#include "performance.hpp"

namespace po = boost::program_options;

boost::contexts::context<> c;

void fn()
{ c.suspend(); }

void test_creation( unsigned int iterations)
{
    cycle_t total( 0);
    cycle_t overhead( get_overhead() );
    std::cout << "overhead for rdtsc == " << overhead << " cycles" << std::endl;

    // cache warm-up
    {
        boost::contexts::protected_stack stack( 2 * 1024 * 1024);
        c = boost::contexts::context<>( fn, boost::move( stack), false);
    }

    for ( unsigned int i = 0; i < iterations; ++i)
    {
        boost::contexts::protected_stack stack( 2 * 1024 * 1024);
        cycle_t start( get_cycles() );
        c = boost::contexts::context<>( fn, boost::move( stack), false);
        cycle_t diff( get_cycles() - start);
        diff -= overhead;
        BOOST_ASSERT( diff >= 0);
        total += diff;
    }
    std::cout << "average of " << total/iterations << " cycles per creation" << std::endl;
}

void test_switching( unsigned int iterations)
{
    cycle_t total( 0);
    cycle_t overhead( get_overhead() );
    std::cout << "overhead for rdtsc == " << overhead << " cycles" << std::endl;

    // cache warum-up
    {
        boost::contexts::protected_stack stack( 2 * 1024 * 1024);
        c = boost::contexts::context<>( fn, boost::move( stack), false);
        c.resume();
        c.resume();
    }

    for ( unsigned int i = 0; i < iterations; ++i)
    {
        boost::contexts::protected_stack stack( 2 * 1024 * 1024);
        c = boost::contexts::context<>( fn, boost::move( stack), false);
        cycle_t start( get_cycles() );
        c.resume();
        cycle_t diff( get_cycles() - start);

        // we have two jumps and two measuremt-overheads
        diff -= overhead; // overhead of measurement
        diff /= 2; // 2x jump_to c1->c2 && c2->c1

        BOOST_ASSERT( diff >= 0);
        total += diff;
    }
    std::cout << "average of " << total/iterations << " cycles per switch" << std::endl;
}

int main( int argc, char * argv[])
{
    try
    {
        unsigned int iterations( 0);

        po::options_description desc("allowed options");
        desc.add_options()
            ("help,h", "help message")
            ("creating,c", "test creation")
            ("switching,s", "test switching")
            ("iterations,i", po::value< unsigned int >( & iterations), "iterations");

        po::variables_map vm;
        po::store(
            po::parse_command_line(
                argc,
                argv,
                desc),
            vm);
        po::notify( vm);

        if ( vm.count("help") )
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        if ( 0 >= iterations) throw std::invalid_argument("iterations must be greater than zero");

        bind_to_processor( 0);

       if ( vm.count("creating") )
           test_creation( iterations);

       if ( vm.count("switching") )
            test_switching( iterations);

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
