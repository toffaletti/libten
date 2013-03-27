#include "stack_alloc.hh"
#include "ten/thread_local.hh"
#include "ten/logging.hh"
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>
#include <sys/mman.h>
#include <algorithm>

namespace ten {

namespace stack_allocator {

size_t default_stacksize{ (size_t)256 * 1024 };
std::atomic<size_t> cache_maxsize{ (size_t)100 };

// impl

namespace {

inline void free_stack(void *ptr, size_t stack_size) {
    PCHECK(munmap(ptr, stack_size) == 0);
}

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

struct cache_tag {};
thread_cached<cache_tag, std::deque<stack>> stack_cache;

} // anon

// calling this function ensures all the above have been initialized
int initialize() { return 0; }


void *allocate(size_t stack_size) {
    // TODO: check stack_size >= min_stacksize (8k because of 4k guard page)
    auto &cache = *stack_cache;

    void *stack_ptr = nullptr;
    if (cache.empty()) {
        stack_ptr = mmap(nullptr, stack_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK, 0, 0);
        if (stack_ptr == MAP_FAILED) {
            throw bad_stack_alloc();
        }
        if (mprotect(stack_ptr, page_size, PROT_NONE) == -1) {
            free_stack(stack_ptr, stack_size);
            throw bad_stack_alloc();
        }
    } else {
        stack_ptr = cache.back().release();
        cache.pop_back();
    }
    return static_cast<char *>(stack_ptr) + stack_size;
}

void deallocate(void *stack_end, size_t stack_size) noexcept {
    void *stack_ptr = static_cast<char *>(stack_end) - stack_size;
    auto &cache = *stack_cache;

    try {
        if (cache.size() < cache_maxsize.load(std::memory_order_relaxed)) {
            cache.emplace_back(stack_ptr, stack_size);
        } else {
            free_stack(stack_ptr, stack_size);
        }
    } catch (std::bad_alloc &e) {
        free_stack(stack_ptr, stack_size);
    }
}

} // stack_allocator

} // ten

