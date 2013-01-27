#ifndef TEN_TASK_HH
#define TEN_TASK_HH

#include "ten/logging.hh"
#include "../src/context.hh"
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <atomic>
#include "t2/kernel.hh"

namespace t2 {

struct task_interrupted {};

struct tasklet {

    context _ctx;
    std::function<void ()> _f;
    std::function<void ()> _post;

    std::mutex _mutex;
    std::weak_ptr<tasklet> _parent;
    std::list<std::shared_ptr<tasklet>> _links;
    std::list<std::shared_ptr<tasklet>>::iterator _link;

    std::atomic<bool> _ready;
    std::atomic<bool> _canceled; 
    bool _unwinding;

    tasklet(std::function<void ()> f)
        : _ctx(tasklet::trampoline),
        _f(std::move(f)),
        _ready(true),
        _canceled(false),
        _unwinding(false)
    {
        DVLOG(5) << "new task: " << this;
        ++ten::the_kernel.taskcount;
    }

    ~tasklet() {
        DVLOG(5) << "freeing task: " << this;
        --ten::the_kernel.taskcount;
    }

    static void cancel(std::shared_ptr<tasklet> t);

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
