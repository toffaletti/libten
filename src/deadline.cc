#include "ten/task.hh"
#include "thread_context.hh"

namespace ten {

struct deadline_pimpl {
    scheduler::alarm_clock::scoped_alarm alarm;
};

deadline::deadline(optional_timeout timeout) {
    if (timeout) {
        if (timeout->count() == 0)
            throw errorx("zero optional_deadline - misuse of optional");
        _set_deadline(*timeout);
    }
}

deadline::deadline(milliseconds ms) {
    _set_deadline(ms);
}

void deadline::_set_deadline(milliseconds ms) {
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        ptr<task::pimpl> t = kernel::current_task();
        auto now = kernel::now();
        _pimpl.reset(new deadline_pimpl{
                this_ctx->scheduler.arm_alarm(t, ms+now, deadline_reached{})
                });
        DVLOG(5) << "deadline alarm armed: " << _pimpl->alarm._armed << " in " << ms.count() << "ms";
    }
}

void deadline::cancel() {
    if (_pimpl) {
        _pimpl->alarm.cancel();
    }
}

deadline::~deadline() {
    cancel();
}

optional_timeout deadline::remaining() const {
    if (_pimpl) {
        return _pimpl->alarm.remaining();
    }
    return nullopt;
}

} // ten
