#ifndef LIBTEN_TASK_STACK_ALLOC_HH_
#define LIBTEN_TASK_STACK_ALLOC_HH_

#include <memory>
#include <atomic>

namespace ten {

struct bad_stack_alloc : std::bad_alloc {
    const char *what() const noexcept override {
        return "ten::bad_stack_alloc";
    }
};

namespace stack_allocator {
    constexpr size_t page_size = 4096;
    constexpr size_t min_stacksize = page_size * 2;

    extern size_t default_stacksize;

    int initialize();

    void *allocate(size_t stack_size);
    void deallocate(void *stack_end, size_t stack_size) noexcept;
};

} // ten

#endif
