#include "ten/task.hh"
#include "thread_context.hh"
#include <chrono_io>

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

void deadline::_set_deadline(kernel::duration dur) {
    if (dur.count() < 0)
        throw_stream() << "negative deadline: " << dur << endx;
    if (dur.count() > 0) {
        const auto t = scheduler::current_task();
        auto now = kernel::now();
        _pimpl.reset(new deadline_pimpl{
                this_ctx->scheduler.arm_alarm(t, now + dur, deadline_reached{})
                });
        DVLOG(5) << "deadline alarm armed: " << _pimpl->alarm._armed << " in " << dur;
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
        // TODO: define optional_timeout in terms of kernel::duration, so this is not lossy
        return duration_cast<optional_timeout::value_type>(_pimpl->alarm.remaining());
    }
    return nullopt;
}

} // ten
