
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if defined(BOOST_WINDOWS) && defined(BOOST_CONTEXT_FIBER)
# include <boost/context/detail/context_base_fiber.hpp>
#elif ! defined(BOOST_WINDOWS) && defined(BOOST_CONTEXT_UCTX)
# include <boost/context/detail/context_base_uctx.hpp>
#elif defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__mips__) || defined(__powerpc__) || defined(_WIN32) || defined(_WIN64)
# include <boost/context/detail/context_base_fctx.hpp>
#else
# error "fcontext_t not supported by this architecture"
#endif
