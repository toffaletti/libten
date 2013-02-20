#ifndef LIBTEN_PROC_HH
#define LIBTEN_PROC_HH

#include "scheduler.hh"
#include "thread_context.hh"

namespace ten {

class proc {
private:
    friend class scheduler;
    ptr<scheduler> _scheduler;
public:
    proc() : _scheduler(&this_ctx->scheduler) {}
    proc(const proc &) = delete;
    proc &operator =(const proc &) = delete;

    void cancel() {
        _scheduler->cancel();
    }

    static void add(ptr<proc> p);
    static void del(ptr<proc> p);

    static void thread_entry(std::shared_ptr<task::pimpl> t);

};

} // end namespace ten

#endif // LIBTEN_PROC_HH
