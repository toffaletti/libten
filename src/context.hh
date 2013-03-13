#ifndef LIBTEN_TASK_CONTEXT_HH
#define LIBTEN_TASK_CONTEXT_HH

#include "stack_alloc.hh"
#include <boost/context/fcontext.hpp>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

namespace ten {

class context {
private:
    stack_allocator allocator;
private:
    struct os_fctx;
    // XXX: defined in thread_context.cc for global ordering
    static thread_cached<os_fctx, boost::context::fcontext_t> _os_ctx;
    boost::context::fcontext_t *_ctx;
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
    explicit context(func_type f, size_t stack_size = 256*1024);

    intptr_t swap(context &other, intptr_t arg=0) noexcept;

    size_t stack_size() const;

    ~context();
};

} // ten

#endif

