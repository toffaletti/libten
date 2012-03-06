#include "ioproc.hh"
#include "address.hh"
#include "logging.hh"

namespace ten {

void ioproctask(iochannel &ch) {
    taskname("ioproctask");
    for (;;) {
        std::unique_ptr<pcall> call;
        try {
            taskstate("waiting for recv");
            call = ch.recv();
        } catch (channel_closed_error &e) {
            taskstate("recv channel closed");
            break;
        }
        if (!call) break;
        taskstate("executing call");
        errno = 0;
        try {
            if (call->vop) {
                DVLOG(5) << "ioproc calling vop";
                call->vop();
                call->vop = 0;
            } else if (call->op) {
                DVLOG(5) << "ioproc calling op";
                call->ret = call->op();
                call->op = 0;
            } else {
                abort();
            }
        } catch (std::exception &e) {
            DVLOG(5) << "ioproc caught exception: " << e.what();
            call->exception = std::current_exception();
        }

        // scope for reply iochannel
        {
            DVLOG(5) << "sending reply";
            iochannel creply = call->ch;
            taskstate("sending reply");
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
