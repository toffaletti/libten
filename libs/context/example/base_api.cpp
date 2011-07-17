
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <boost/context/all.hpp>

boost_fcontext_t fcm, fc1, fc2;

void f1( void * p)
{
        (void) p;
        fprintf(stderr,"f1() stated\n");
        fprintf(stderr,"f1: call boost_fcontext_jump( & fc1, & fc2)\n");
        boost_fcontext_jump( & fc1, & fc2);
        fprintf(stderr,"f1() returns\n");
}

void f2( void * p)
{
        (void) p;
        fprintf(stderr,"f2() stated\n");
        fprintf(stderr,"f2: call boost_fcontext_jump( & fc2, & fc1)\n");
        boost_fcontext_jump( & fc2, & fc1);
        fprintf(stderr,"f2() returns\n");
}

int main( int argc, char * argv[])
{
        boost::contexts::protected_stack b1(262144);
        boost::contexts::protected_stack b2(262144);

#if defined(BOOST_WINDOWS) && defined(_M_X64)
        std::vector< char > bf1( 528, '\0');
        std::vector< char > bf2( 528, '\0');
        std::vector< char > bfm( 528, '\0');

        fcm.fc_fp = reinterpret_cast< char * >(
            ( ( reinterpret_cast< uint64_t >( & bfm[0]) + 16) >> 4) << 4 );
#endif

        fc1.fc_stack.ss_base = b1.address();
        fc1.fc_stack.ss_limit =
            static_cast< char * >( fc1.fc_stack.ss_base) - b1.size();
        fc1.fc_link = & fcm;
#if defined(BOOST_WINDOWS) && defined(_M_X64)
        fc1.fc_fp = reinterpret_cast< char * >(
            ( ( reinterpret_cast< uint64_t >( & bf1[0]) + 16) >> 4) << 4 );
#endif
        boost_fcontext_make( & fc1, f1, NULL);

        fc2.fc_stack.ss_base = b2.address();
        fc2.fc_stack.ss_limit =
            static_cast< char * >( fc2.fc_stack.ss_base) - b2.size();
#if defined(BOOST_WINDOWS) && defined(_M_X64)
        fc2.fc_fp = reinterpret_cast< char * >(
            ( ( reinterpret_cast< uint64_t >( & bf2[0]) + 16) >> 4) << 4 );
#endif
        boost_fcontext_make( & fc2, f2, NULL);

        fprintf(stderr,"main: call boost_fcontext_jump( & fcm, & fc1)\n");
        boost_fcontext_jump( & fcm, & fc1);

        fprintf( stderr, "main() returns\n");
        return EXIT_SUCCESS;
}
