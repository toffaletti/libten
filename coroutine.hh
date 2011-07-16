#ifndef COROUTINE_HH
#define COROUTINE_HH
#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <ucontext.h>
#include <sys/mman.h>

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

namespace b {

class coroutine : boost::noncopyable {
public:
    typedef void (*proc)(void *);

    coroutine();
    coroutine(proc f_, void *arg=NULL, size_t stack_size_=4096);
    ~coroutine();

    void swap(coroutine *to);
private:
    ucontext_t context;
#ifndef NVALGRIND
    int valgrind_stack_id;
#endif
    char *stack;
    size_t stack_size;
};

} // end namespace b

#include "thread.hh"

#include <iostream>

// setjmp/longjmp:
// http://stackoverflow.com/questions/4352451/coroutine-demo-source-2

// http://mfichman.blogspot.com/2011/05/lua-style-coroutines-in-c.html

namespace scheduler {

class thread;

class coroutine : boost::noncopyable {
public:
    typedef boost::function<void ()> func_t;

    static coroutine *self();
    static void swap(coroutine *from, coroutine *to);
    static coroutine *spawn(const func_t &f);

    static void yield();
    static void migrate();
    static void migrate_to(thread *to);

protected:
    friend class thread;
    coroutine() : exiting(false), stack(0), stack_size(0) {
        //std::cout << "main coro context: " << &context << "\n";
        getcontext(&context);
    }

    ~coroutine() {
        if (stack) {
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(valgrind_stack_id);
#endif
            THROW_ON_ERROR(mprotect(stack+stack_size, getpagesize(), PROT_READ|PROT_WRITE));
            free(stack);
        }
    }

    void switch_();

private:
    ucontext_t context;
    func_t f;
    bool exiting;
#ifndef NVALGRIND
    int valgrind_stack_id;
#endif
    char *stack;
    size_t stack_size;

    coroutine(const func_t &f_, size_t stack_size_=4096*2);

    static void start(coroutine *);
};

} // end namespace scheduler

#endif // COROUTINE_HH
