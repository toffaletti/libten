#ifndef LIBTEN_TASK_STACK_ALLOC_HH_
#define LIBTEN_TASK_STACK_ALLOC_HH_

#include "ten/thread_local.hh"
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>
#include <sys/mman.h>
#include <algorithm>

namespace ten {

struct bad_stack_alloc : std::bad_alloc {
    const char *what() const noexcept override {
        return "ten::bad_stack_alloc";
    }
};

class stack_allocator {
public:
    static constexpr size_t page_size = 4096;
    static constexpr size_t min_stacksize = page_size * 2;
    static constexpr size_t default_stacksize = 256*1024;

    // TODO: use better logic that is based on the total
    // size of memory used by the stacks in the cache
    static constexpr size_t max_cached_stacks = 100;

private:
    struct stack {
        void *ptr = nullptr;
        size_t size = 0;

        stack(void *p, size_t s) : ptr{p}, size{s} {}

        stack(const stack &) = delete;
        stack & operator = (const stack &) = delete;

        stack(stack &&other)               { take(other); }
        stack & operator = (stack &&other) { if (this != &other) take(other); return *this; }

        ~stack() { clear(); }

        void take(stack &other) {
            clear();
            std::swap(ptr, other.ptr);
            std::swap(size, other.size);
        }
        void clear() {
            if (ptr) {
                free_stack(ptr, size);
                ptr = nullptr;
            }
        }

        void *release() {
            void *tmp = nullptr;
            std::swap(tmp, ptr);
            return tmp;
        }
    };

    struct stack_cache {};
    //XXX this is defined in thread_context.cc for ordering reasons
    static thread_cached<stack_cache, std::vector<stack>> _cache;

    static void free_stack(void *ptr, size_t stack_size) {
        PCHECK(munmap(ptr, stack_size) == 0);
    }

public:
    void *allocate(size_t stack_size) {
        // TODO: check stack_size >= min_stacksize (8k because of 4k guard page)
        auto &cache = *_cache;
        auto ri = std::find_if(cache.rbegin(), cache.rend(),
                               [=](const stack &s) { return stack_size == s.size; });
        void *stack_ptr = nullptr;
        if (ri != cache.rend()) {
            auto i = ri.base() - 1;
            stack_ptr = i->release();
            cache.erase(i);
        } else {
            stack_ptr = mmap(nullptr, stack_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK, 0, 0);
            if (stack_ptr == MAP_FAILED) {
                throw bad_stack_alloc();
            }
            if (mprotect(stack_ptr, page_size, PROT_NONE) == -1) {
                free_stack(stack_ptr, stack_size);
                throw bad_stack_alloc();
            }
        }
        return reinterpret_cast<void *>(reinterpret_cast<char *>(stack_ptr) + stack_size);
    }

    void deallocate(void *stack_end, size_t stack_size) noexcept {
        void *stack_ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(stack_end) - stack_size);
        auto &cache = *_cache;
        if (cache.size() < max_cached_stacks) {
            cache.emplace_back(stack_ptr, stack_size);
        } else {
            free_stack(stack_ptr, stack_size);
        }
    }
};

} // ten

#endif
