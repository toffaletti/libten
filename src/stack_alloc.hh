#ifndef LIBTEN_TASK_STACK_ALLOC_HH_
#define LIBTEN_TASK_STACK_ALLOC_HH_

#include "ten/thread_local.hh"
#include "ten/logging.hh"
#include <sys/mman.h>
#include <algorithm>
#include <list>

namespace ten {

class stack_allocator {
public:
    static constexpr size_t page_size = 4096;
private:
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
    //XXX this is defined in thread_context.cc for ordering reasons
    static thread_cached<stack_cache, std::list<stack>> _cache;

public:
    void *allocate(size_t stack_size) noexcept {
        std::list<stack> &cache = *_cache.get();
        auto i = std::find_if(begin(cache), end(cache), [=](const stack &s) -> bool {
                return stack_size == s.size;
                });
        size_t real_size = stack_size + page_size;
        void *stack_end = nullptr;
        if (i != end(cache)) {
            stack_end = i->release();
            cache.erase(i);
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

    void deallocate(void *p, size_t stack_size) noexcept {
        void *stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(p) - stack_size - page_size);
        _cache->emplace_back(stack_end, stack_size);
    }
};

} // ten

#endif
