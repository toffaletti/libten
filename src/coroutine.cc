#include "ten/task2/coroutine.hh"

boost::ctx::stack_allocator coroutine::allocator;

namespace this_coro {
namespace {
    __thread coroutine *current_coro = nullptr;
} // end anon namespace

coroutine *get() noexcept {
    return current_coro;
}

void yield() noexcept {
    current_coro->yield();
}

bool yield_to(coroutine &other) noexcept {
    return current_coro->yield_to(other);
}

} // end namespace this_coro

coroutine::coroutine() noexcept {
    this_coro::current_coro = this;
}

bool coroutine::yield_to(coroutine &other) noexcept {
    if (!other._f) return false;
    if (this == &other) return false;
    other._parent = this;
    this_coro::current_coro = &other;

    boost::ctx::jump_fcontext(&_ctx,
            &other._ctx,
            reinterpret_cast<intptr_t>(&other));

    return true;
}

void coroutine::yield() noexcept {
    if (_parent != nullptr) {
        // return to caller
        this_coro::current_coro = _parent;

        boost::ctx::jump_fcontext(&_ctx,
                &_parent->_ctx,
                reinterpret_cast<intptr_t>(_parent));
    }
}

void coroutine::join() noexcept {
    while (this_coro::yield_to(*this)) {}
}

