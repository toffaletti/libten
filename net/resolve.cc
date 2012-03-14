#include <netdb.h>
#include "net.hh"
#include "ioproc.hh"

namespace ten {

static int _dial(int fd, const char *addr, uint16_t port) {
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

template <typename ProcT> int iodial(ProcT &io, int fd, const char *addr, uint64_t port) {
    return iocall<int>(io, std::bind(_dial, fd, addr, port));
}

// TODO: maybe use a single ioproc per thread (proc)
int netdial(int fd, const char *addr, uint16_t port) {
    // need large stack size for getaddrinfo (see _dial)
    ioproc io(8*1024*1024);
    return iodial(io, fd, addr, port);
}

void netinit() {
    // called once per process
}

} // end namespace ten
