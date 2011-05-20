#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <ucontext.h>

#include <iostream>

// setjmp/longjmp:
// http://stackoverflow.com/questions/4352451/coroutine-demo-source-2

class coroutine : boost::noncopyable {
public:
    typedef boost::function<void (coroutine &)> start_func_t;

    coroutine(start_func_t f_, size_t stack_size=4096)
        : f(f_), stack(stack_size), done(false)
    {
        getcontext(&context);
        context.uc_stack.ss_sp = &stack[0];
        context.uc_stack.ss_size = stack.size();
        context.uc_link = 0;
        makecontext(&context, (void (*)())coroutine::start, 1, this);
    }

    coroutine() {
        getcontext(&context);
    }

    bool swap(coroutine &other) {
        if (other.done) return false;
        int r = swapcontext(&other.caller, &other.context);
        return true;
    }

    void yield() {
        int r = swapcontext(&context, &caller);
    }

protected:
    ucontext_t context;
    start_func_t f;
    std::vector<char> stack;
    ucontext_t caller;
    bool done;

    static void start(coroutine *self) {
        try {
            self->f(*self);
        } catch(...) {
        }
        self->done = true;
        self->yield();
    }
};