#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <pthread.h>

class thread : boost::noncopyable {
public:
    typedef boost::function<void ()> func_t;

    static thread &spawn(const func_t &f) {
        return *(new thread(f));
    }

private:
    pthread_t t;
    func_t f;

    thread(const func_t &f_) : f(f_) {
        pthread_create(&t, NULL, start, this);
    }

    static void *start(void *arg) {
        thread *t = (thread *)arg;
        try {
            t->f();
        } catch (...) {
        }
        return NULL;
    }
};