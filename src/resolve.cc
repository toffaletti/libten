#include <netdb.h>
#include "ten/net.hh"
#include "ten/ioproc.hh"

namespace ten {

static int _dial(int fd, const char *addr, uint16_t port) {
    struct addrinfo *results = 0;
    struct addrinfo *result = 0;
    int status = getaddrinfo(addr, NULL, NULL, &results);
    if (status == 0) {
        for (result = results; result != NULL; result = result->ai_next) {
            address addr(result->ai_addr, result->ai_addrlen);
            addr.port(port);
            status = netconnect(fd, addr, {});
            if (status == 0) break;
        }
    }
    freeaddrinfo(results);
    return status;
}

template <typename ProcT> int iodial(ProcT &io, int fd, const char *addr, uint64_t port) {
    return iocall(io, std::bind(_dial, fd, addr, port));
}

void netdial(int fd, const char *addr, uint16_t port, optional_timeout connect_ms) {
    // need large stack size for getaddrinfo (see _dial)
    static ioproc io(8*1024*1024);
    if (iodial(io, fd, addr, port) != 0) {
        // TODO: better error handling
        throw hostname_error("dns error"); 
    }
}

void netinit() {
    // called once per process
}

} // end namespace ten
