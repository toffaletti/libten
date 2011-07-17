
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if defined(BOOST_WINDOWS)
# if defined(BOOST_CONTEXT_FIBER)
#  include <boost/context/detail/context_base_fiber.hpp>
# else
#  if defined(_M_IX86)
#   include <boost/context/detail/context_base_fctx_win32.hpp>
#  elif defined(_M_X64)
#   include <boost/context/detail/context_base_fctx_win64.hpp>
#  else
#   error "fcontext_t not supported by this architecture"
#  endif
# endif
#else
# if defined(BOOST_CONTEXT_UCTX)
#  include <boost/context/detail/context_base_uctx.hpp>
# else
#  if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__mips__) || defined(__powerpc__)
#   include <boost/context/detail/context_base_fctx_posix.hpp>
#  else
#   error "fcontext_t not supported by this architecture"
#  endif
# endif
#endif
