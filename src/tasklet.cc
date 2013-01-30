#include "t2/scheduler.hh"

namespace t2 {

void tasklet::cancel(std::shared_ptr<tasklet> t) {
    CHECK(t);
    DVLOG(5) << "canceling " << t;
    t->_canceled = true;
    if (t->_ready.exchange(true) == false) {
        the_scheduler->add(std::move(t));
    }
}


void tasklet::yield() {
    tld->self->_yield();
}

void tasklet::_yield() {
    _ctx.swap(tld->ctx, 0);
    if (_canceled && _unwinding.exchange(true) == false) {
        throw task_interrupted{};
    }
}

bool tasklet::swap() {
    if (_f) {
        _ready = false;
        tld->ctx.swap(_ctx, reinterpret_cast<intptr_t>(this));
        if (_canceled && _unwinding.exchange(true) == false) {
            throw task_interrupted{};
        }
    }
    return static_cast<bool>(_f);
}

void tasklet::trampoline(intptr_t arg) noexcept {
    tasklet *self = reinterpret_cast<tasklet *>(arg);
    try {
        self->_f();
    } catch (task_interrupted &e) {}
    DVLOG(5) << "task func finished: " << self;
    self->_f = nullptr;
    {
        auto parent = self->_parent.lock();
        // TODO: maybe propogate exceptions?
        if (parent) {
            // TODO: adding this causes all kinds of bad
            //tasklet::cancel(parent);
            std::lock_guard<std::mutex> lock(parent->_mutex);
            parent->_links.erase(self->_link);
        }

        std::lock_guard<std::mutex> lock(self->_mutex);
        for (auto &child: self->_links) {
            tasklet::cancel(child);
        }
    }
    self->_ctx.swap(tld->ctx, 0);
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

void tasklet::_spawn(std::function<void ()> f) {
    auto t = std::make_shared<tasklet>(std::move(f));
    the_scheduler->add(std::move(t));
}

void tasklet::_spawn_link(std::function<void ()> f) {
    auto t = std::make_shared<tasklet>(std::move(f));
    CHECK(tld->self);
    {
        std::lock_guard<std::mutex> lock(tld->self->_mutex);
        t->_parent = tld->self;
        tld->self->_links.push_front(t);
        t->_link = tld->self->_links.begin();
    }
    the_scheduler->add(std::move(t));
}

} // t2
