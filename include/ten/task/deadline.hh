#ifndef TEN_TASK_DEADLINE_HH
#define TEN_TASK_DEADLINE_HH

#include "ten/task/task.hh"

namespace ten {

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

// forward decl
struct deadline_pimpl;

//! schedule a deadline to interrupt task with
//! deadline_reached after milliseconds
class deadline {
private:
    std::shared_ptr<deadline_pimpl> _pimpl;
public:
    deadline(std::chrono::milliseconds ms);

    deadline(const deadline &) = delete;
    deadline &operator =(const deadline &) = delete;

    //! milliseconds remaining on the deadline
    std::chrono::milliseconds remaining() const;
    
    //! cancel the deadline
    void cancel();

    ~deadline();
};

} // ten
#endif
