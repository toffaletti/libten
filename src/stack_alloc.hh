#ifndef LIBTEN_TASK_STACK_ALLOC_HH_
#define LIBTEN_TASK_STACK_ALLOC_HH_

#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>

namespace ten {

class stack_allocator {
public:
    static constexpr size_t page_size = 4096;

public:
    void *allocate(size_t stack_size) noexcept;
    void deallocate(void *p, size_t stack_size) noexcept;
};

} // ten

#endif
