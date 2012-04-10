#ifndef SEMA_HH
#define SEMA_HH

#include "error.hh"
#include <boost/utility.hpp>
#include <semaphore.h>

namespace ten {

//! wrapper around anonymous POSIX semaphore
class semaphore : boost::noncopyable {
public:
    explicit semaphore(unsigned int value = 0) {
        THROW_ON_ERROR(sem_init(&s, 0, value));
    }

    //! increment (unlock) the semaphore
    void post() {
        THROW_ON_ERROR(sem_post(&s));
    }

    //! decrement (lock) the semaphore
    //
    //! blocks if the value drops below 0
    void wait() {
        THROW_ON_ERROR(sem_wait(&s));
    }

    // TODO: sem_trywait and sem_timedwait

    ~semaphore() {
        THROW_ON_ERROR(sem_destroy(&s));
    }
protected:
    sem_t s;
};

} // end namespace ten

#endif // SEMA_HH
