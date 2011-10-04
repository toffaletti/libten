#ifndef ADDRESS_HH
#define ADDRESS_HH

#include "error.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ostream>
#include <boost/lexical_cast.hpp>

namespace fw {

inline void parse_host_port(std::string &host, unsigned short &port) {
    size_t pos = host.rfind(':');
    if (pos != std::string::npos) {
        port = boost::lexical_cast<unsigned short>(host.substr(pos+1, -1));
        host = host.substr(0, pos);
    }
}

//! union of sockaddr structs
union address_u {
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
};

//! wrapper around sockaddr structures that strives to
//! help make ipv6 support seemless.
struct address {
    union address_u addr;

    //! construct an empty address
    address() {
        clear();
    }

    address(struct sockaddr *a, socklen_t alen) {
        clear();
        memcpy(&addr.sa, a, alen);
    }

    //! \param s address in string format for inet_pton
    //! \param p port in host byte order
    address(const char *s, uint16_t p=0) {
        clear();
        pton(s); // can throw
        port(p);
    }

    //! zeros out the address
    void clear() {
        memset(&addr, 0, sizeof(addr));
        addr.sa.sa_family = AF_INET; // get rid of valgrind warning about unitialized memory
    }

    //! \return sizeof(struct sockaddr_in) or sizeof(struct sockaddr_in6) depending on the value of family()
    socklen_t addrlen() const {
        return family() == AF_INET ? sizeof(addr.sa_in) : sizeof(addr.sa_in6);
    }

    //! \return a struct sockaddr pointer to the backing storage
    struct sockaddr *sockaddr() const {
        // cast away const cause i'm evil
        return (struct sockaddr *)&addr.sa;
    }

    //! sets address family AF_INET or AF_INET6
    void family(int f) { addr.sa.sa_family = f; }
    //! \return AF_INET or AF_INET6
    int family() const { return addr.sa.sa_family; }

    //! sets port
    //! \param p port in host by order
    void port(uint16_t p) {
        if (family() == AF_INET) {
            addr.sa_in.sin_port = htons(p);
        } else if (family() == AF_INET6) {
            addr.sa_in6.sin6_port = htons(p);
        }
    }

    //! returns port in host byte order
    uint16_t port() const {
        return ntohs(family() == AF_INET ? addr.sa_in.sin_port : addr.sa_in6.sin6_port);
    }

    //! wrapper around inet_ntop
    const char *ntop(char *dst, socklen_t size) const {
        const char *rvalue = NULL;
        if (family() == AF_INET) {
            rvalue = inet_ntop(family(), &addr.sa_in.sin_addr, dst, size);
            THROW_ON_NULL(rvalue);
        } else if (family() == AF_INET6) {
            rvalue = inet_ntop(family(), &addr.sa_in6.sin6_addr, dst, size);
            THROW_ON_NULL(rvalue);
        }
        return rvalue;
    }

    //! tries inet_pton() first with AF_INET and then AF_INET6
    void pton(const char *s) {
        if (inet_pton(AF_INET, s, &addr.sa_in.sin_addr) == 1) {
            addr.sa.sa_family = AF_INET;
            return;
        } else if (inet_pton(AF_INET6, s, &addr.sa_in6.sin6_addr) == 1) {
            addr.sa.sa_family = AF_INET6;
            return;
        }
        throw errno_error();
    }

    //! output address in addr:port format
    friend std::ostream &operator<< (std::ostream &out, const address &addr) {
        char buf[INET6_ADDRSTRLEN];
        out << addr.ntop(buf, sizeof(buf)) << ":" << addr.port();
        return out;
    }
};

} // end namespace fw

#endif // ADDRESS_HH

