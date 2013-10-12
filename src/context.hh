#ifndef LIBTEN_TASK_CONTEXT_HH
#define LIBTEN_TASK_CONTEXT_HH

#include "stack_alloc.hh"
#include "ten/thread_local.hh"
#include <boost/version.hpp>

#if BOOST_VERSION == 105100
    #include <boost/context/fcontext.hpp>
    #include <boost/context/stack_utils.hpp>
    namespace ctx = boost::ctx;
#elif BOOST_VERSION >= 105200
    #include <boost/context/fcontext.hpp>
    namespace ctx = boost::context;
#endif

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

namespace ten {

class context {
private:
    struct os_fctx;
    // XXX: defined in thread_context.cc for global ordering
    static thread_cached<os_fctx, ctx::fcontext_t> _os_ctx;
    ctx::fcontext_t *_ctx;
#ifndef NVALGRIND
    //! stack id so valgrind doesn't freak when stack swapping happens
    int valgrind_stack_id;
#endif
public:
    typedef void (*func_type)(intptr_t);
public:
    context(const context &) = delete;
    context(const context &&) = delete;

    //! make context for existing stack
    context() noexcept;

    //! make a new context and stack
    explicit context(func_type f, size_t stack_size);

    intptr_t swap(context &other, intptr_t arg=0) noexcept;

    size_t stack_size() const;

    ~context();
};

} // ten

#endif

