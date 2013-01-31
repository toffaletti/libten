#ifndef LIBTEN_SEMAPHORE_HH
#define LIBTEN_SEMAPHORE_HH

#include "ten/logging.hh"
#include "ten/error.hh"
#include <semaphore.h>

namespace ten {

//! wrapper around anonymous POSIX semaphore
class semaphore {
private:
    sem_t _s;
public:
    explicit semaphore(unsigned int value = 0) {
        throw_if(sem_init(&_s, 0, value) == -1);
    }

    semaphore(const semaphore &) = delete;
    semaphore &operator =(const semaphore &) = delete;

    //! increment (unlock) the semaphore
    void post() {
        throw_if(sem_post(&_s) == -1);
    }

    //! decrement (lock) the semaphore
    //
    //! blocks if the value drops below 0
    void wait() {
        throw_if(sem_wait(&_s) == -1);
    }

    // TODO: sem_trywait and sem_timedwait

    ~semaphore() {
        PCHECK(sem_destroy(&_s) == 0);
    }
};

} // end namespace ten

#endif // LIBTEN_SEMAPHORE_HH
