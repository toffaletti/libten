#include "ten/logging.hh"
#include "ten/thread_local.hh"
#include "stack_alloc.hh"
#include <sys/mman.h>
#include <algorithm>

namespace ten {

namespace  {
    struct stack {
        void *ptr;
        size_t size;

        void *release() {
            void *tmp = nullptr;
            std::swap(tmp, ptr);
            return tmp;
        }

        stack(void *p, size_t s) : ptr{p}, size{s} {}

        ~stack() {
            if (ptr) {
                PCHECK(mprotect(ptr, stack_allocator::page_size, PROT_READ|PROT_WRITE) == 0);
                free(ptr);
            }
        }
    };

    struct stack_cache {};
    static thread_cached<stack_cache, std::list<stack>> cache;
}

void *stack_allocator::allocate(size_t stack_size) noexcept {
    auto i = std::find_if(cache->begin(), cache->end(), [=](const stack &s) -> bool {
            return stack_size == s.size;
            });
    size_t real_size = stack_size + page_size;
    void *stack_end = nullptr;
    if (i != cache->end()) {
        stack_end = i->release();
        cache->erase(i);
    } else {
        int r = posix_memalign(&stack_end, page_size, real_size);
        CHECK(r == 0) << "could not allocate stack " << r;
        // protect the guard page
        PCHECK(mprotect(stack_end, page_size, PROT_NONE) == 0);
    }
    stack_end = reinterpret_cast<void *>(
            reinterpret_cast<intptr_t>(stack_end) + stack_size + page_size);
    return stack_end;
}

void stack_allocator::deallocate(void *p, size_t stack_size) noexcept {
    void *stack_end = reinterpret_cast<void *>(
            reinterpret_cast<intptr_t>(p) - stack_size - page_size);
    cache->emplace_back(stack_end, stack_size);

}


} // ten

