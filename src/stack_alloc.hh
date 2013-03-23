#ifndef LIBTEN_TASK_STACK_ALLOC_HH_
#define LIBTEN_TASK_STACK_ALLOC_HH_

#include "ten/thread_local.hh"
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>
#include <sys/mman.h>
#include <algorithm>

namespace ten {

// TODO: should this be public?
class bad_stack_alloc : public std::bad_alloc {
public:
    bad_stack_alloc() {}
    const char *what() const noexcept override {
        return "ten::bad_stack_alloc";
    }
};

class stack_allocator {
public:
    static constexpr size_t page_size = 4096;
    static constexpr size_t min_stacksize = page_size * 2;
    static constexpr size_t default_stacksize = 256*1024;
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
                free_stack(ptr, size);
            }
        }
    };

    struct stack_cache {};
    //XXX this is defined in thread_context.cc for ordering reasons
    static thread_cached<stack_cache, std::list<stack>> _cache;

    static void free_stack(void *ptr, size_t stack_size) {
        PCHECK(munmap(ptr, stack_size) == 0);
    }

public:
    void *allocate(size_t stack_size) {
        // TODO: check stack_size >= min_stacksize (8k because of 4k guard page)
        std::list<stack> &cache = *_cache.get();
        auto i = std::find_if(begin(cache), end(cache), [=](const stack &s) -> bool {
                return stack_size == s.size;
                });
        void *stack_end = nullptr;
        if (i != end(cache)) {
            stack_end = i->release();
            cache.erase(i);
        } else {
            stack_end = mmap(nullptr, stack_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK, 0, 0);
            if (stack_end == MAP_FAILED) {
                throw bad_stack_alloc();
            }
            mprotect(stack_end, page_size, PROT_NONE);
            // allow mprotect to fail, living without a guard page is ok
        }
        stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(stack_end) + stack_size);
        return stack_end;
    }

    void deallocate(void *p, size_t stack_size) noexcept {
        void *stack_end = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(p) - stack_size);
        // TODO: use better logic that is based on the total
        // size of memory used by the stacks in the cache
        if (_cache->size() < 100) {
            _cache->emplace_back(stack_end, stack_size);
        } else {
            free_stack(stack_end, stack_size);
        }
    }
};

} // ten

#endif
