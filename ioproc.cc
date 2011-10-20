#include "ioproc.hh"
#include "address.hh"
#include "logging.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>

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
                taskstate("send channel closed");
                break;
            }
        }
    }
    DVLOG(5) << "exiting ioproc";
}

int netconnect(int fd, const address &addr, unsigned int ms) {
    while (::connect(fd, addr.sockaddr(), addr.addrlen()) < 0) {
        if (errno == EINTR)
            continue;
        if (errno == EINPROGRESS || errno == EADDRINUSE) {
            errno = 0;
            if (fdwait(fd, 'w', ms)) {
                return 0;
            } else if (errno == 0) {
                errno = ETIMEDOUT;
            }
        }
        return -1;
    }
    return 0;
}

int dial(int fd, const char *addr, uint64_t port) {
    struct addrinfo *results = 0;
    struct addrinfo *result = 0;
    int status = getaddrinfo(addr, NULL, NULL, &results);
    if (status == 0) {
        for (result = results; result != NULL; result = result->ai_next) {
            address addr(result->ai_addr, result->ai_addrlen);
            addr.port(port);
            status = netconnect(fd, addr, 0);
            if (status == 0) break;
        }
    }
    freeaddrinfo(results);
    return status;
}

} // end namespace fw
