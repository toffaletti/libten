#include "t2/task.hh"
#include "t2/channel.hh"

struct thread_data {
    context ctx;
    std::shared_ptr<t2::tasklet> self;
};

extern t2::channel<std::shared_ptr<t2::tasklet>> sched_chan;
extern __thread thread_data *tld;

namespace t2 {

void tasklet::yield() {
    tld->self->_yield();
}

void tasklet::_yield() {
    _ctx.swap(tld->ctx, 0);
}

bool tasklet::swap() {
    if (_f) {
        tld->ctx.swap(_ctx, reinterpret_cast<intptr_t>(this));
    }
    return static_cast<bool>(_f);
}

void tasklet::trampoline(intptr_t arg) noexcept {
    tasklet *self = reinterpret_cast<tasklet *>(arg);
    self->_f();
    self->_f = nullptr;
    {
        auto parent = self->_parent.lock();
        // TODO: maybe propogate exceptions?
        if (parent) {
            //parent->cancel();
            //std::find(begin(parent->_links), end(parent->_links), self)
        }

        for (auto &link: self->_links) {
            auto child = link.lock();
            if (child) {
                child->cancel();
            }
        }
    }
    self->_ctx.swap(tld->ctx, 0);
    LOG(FATAL) << "Oh no! You fell through the trampoline " << self;
}

void tasklet::_spawn(std::function<void ()> f) {
    auto t = std::make_shared<tasklet>(std::move(f));
    sched_chan.send(std::move(t));
}

void tasklet::_spawn_link(std::function<void ()> f) {
    auto t = std::make_shared<tasklet>(std::move(f));
    CHECK(tld->self);
    //t->_parent = tld->self;
    //tld->self->_links.push_back(t);
    sched_chan.send(std::move(t));
}

} // t2
