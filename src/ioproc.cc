#include "ten/ioproc.hh"
#include "ten/address.hh"
#include "ten/logging.hh"

namespace ten {

void ioproctask(iochannel &ch) {
    this_task::name("ioproctask");
    for (;;) {
        std::unique_ptr<pcall> call;
        try {
            this_task::state("waiting for recv");
            call = ch.recv();
        } catch (channel_closed_error &e) {
            this_task::state("recv channel closed");
            break;
        }
        if (!call) break;
        this_task::state("executing call");
        errno = 0;
        try {
            DVLOG(5) << "ioproc calling op";
            call->ret = call->op();
            call->op = 0;
        } catch (std::exception &e) {
            DVLOG(5) << "ioproc caught exception: " << e.what();
            call->exception = std::current_exception();
        }

        // scope for reply iochannel
        {
            DVLOG(5) << "sending reply";
            iochannel creply = call->ch;
            this_task::state("sending reply");
            try {
                creply.send(std::move(call));
            } catch (channel_closed_error &e) {
                // ignore this
            }
            DVLOG(5) << "done sending reply";
        }
    }
    DVLOG(5) << "exiting ioproc";
}


} // end namespace ten
