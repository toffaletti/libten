#include "ioproc.hh"
#include "address.hh"
#include "logging.hh"

namespace fw {

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

        // scope for reply iochannel
        {
            iochannel creply = call->ch;
            taskstate("sending reply");
            try {
                creply.send(call);
            } catch (channel_closed_error &e) {
                // ignore this
            }
        }
    }
    DVLOG(5) << "exiting ioproc";
}


} // end namespace fw
