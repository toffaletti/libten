#ifndef COROUTINE_HH
#define COROUTINE_HH
#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <sys/mman.h>
#include "context.hh"

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif

class coroutine : boost::noncopyable {
public:
    typedef void (*proc)(void *);

    coroutine();
    coroutine(proc f_, void *arg=NULL, size_t stack_size_=4096);
    ~coroutine();

    void swap(coroutine *to);
private:
    context ctxt;
#ifndef NVALGRIND
    int valgrind_stack_id;
#endif
    char *stack;
    size_t stack_size;
};

#endif // COROUTINE_HH
