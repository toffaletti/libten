#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <ucontext.h>

#include <iostream>

// setjmp/longjmp:
// http://stackoverflow.com/questions/4352451/coroutine-demo-source-2

// http://mfichman.blogspot.com/2011/05/lua-style-coroutines-in-c.html

const size_t stack_size = 4096;

class coroutine : boost::noncopyable {
public:
    typedef boost::function<void ()> func_t;

    bool swap_(coroutine &other) {
        if (other.done) { delete &other; return false; }
        coroutine *saved = caller_;
        caller_ = this;
        callee_ = &other;
        int r = swapcontext(&context, &other.context);
        caller_ = saved;
        callee_ = this;
        return true;
    }

    static coroutine &self() { return *callee_; }

    static bool swap(coroutine &other) {
        return self().swap_(other);
    }

    static void yield() {
        assert(callee_ != NULL);
        assert(caller_ != NULL);
        int r = swapcontext(&callee_->context, &caller_->context);
    }

    static coroutine &spawn(const func_t &f) {
        if (callee_ == NULL) {
            callee_ = new coroutine();
        }
        return *(new coroutine(f));
    }

private:
    static coroutine *caller_;
    static coroutine *callee_;

    ucontext_t context;
    func_t f;
    bool done;
    char stack[stack_size];

    coroutine(const func_t &f_)
        : f(f_), done(false)
    {
        getcontext(&context);
        context.uc_stack.ss_sp = &stack[0];
        context.uc_stack.ss_size = stack_size;
        context.uc_link = 0;
        makecontext(&context, (void (*)())coroutine::start, 1, this);
    }

    coroutine() {
        getcontext(&context);
    }

    static void start(coroutine *c) {
        try {
            c->f();
        } catch(...) {
        }
        c->done = true;
        coroutine::yield();
    }
};

coroutine *coroutine::caller_ = NULL;
coroutine *coroutine::callee_ = NULL;