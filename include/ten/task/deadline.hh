#ifndef LIBTEN_TASK_DEADLINE_HH
#define LIBTEN_TASK_DEADLINE_HH

namespace ten {

// inherit from task_interrupted so lock/rendez/poll canceling
// doesn't need to be duplicated
struct deadline_reached : task_interrupted {};

// forward decl
struct deadline_pimpl;

//! schedule a deadline to interrupt task with
//! deadline_reached exception after N milliseconds
class deadline {
private:
    std::unique_ptr<deadline_pimpl> _pimpl;
    void _set_deadline(std::chrono::milliseconds ms);
public:
    deadline(optional_timeout timeout);
    deadline(std::chrono::milliseconds ms); // deprecated

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
