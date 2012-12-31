#ifndef TEN_TASK_HH
#define TEN_TASK_HH

#include "ten/logging.hh"
#include "../src/context.hh"
#include <functional>
#include <vector>
#include <memory>
#include <atomic>

extern std::atomic<uint64_t> taskcount;

namespace t2 {

struct tasklet {

    context _ctx;
    std::function<void ()> _f;
    std::function<void ()> _post;
    std::weak_ptr<tasklet> _parent;
    std::vector<std::weak_ptr<tasklet>> _links;
    std::atomic_flag _canceled; 

    tasklet(std::function<void ()> f)
        : _ctx(tasklet::trampoline),
        _f(std::move(f))
    {
        ++taskcount;
    }

    ~tasklet() {
        DVLOG(5) << "freeing task: " << this;
        --taskcount;
    }

    void cancel() {
        _canceled.test_and_set();
    }

    static void yield();

    void _yield();

    bool swap();

    static void trampoline(intptr_t arg) noexcept;

    template <class Func, class ...Args>
    static void spawn(Func &&f, Args&&... args) {
        _spawn(std::bind(f, args...));
    }

    template <class Func, class ...Args>
    static void spawn_link(Func &&f, Args&&... args) {
        _spawn_link(std::bind(f, args...));
    }

    static void _spawn(std::function<void ()> f);
    static void _spawn_link(std::function<void ()> f);
};



}

#endif
