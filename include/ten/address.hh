#ifndef TEN_ADDRESS_HH
#define TEN_ADDRESS_HH

#include "error.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace ten {

inline void parse_host_port(std::string &host, uint16_t &port) {
    size_t pos = host.rfind(':');
    if (pos != std::string::npos) {
        const auto hp = host.substr(pos + 1);
        try {
            port = boost::lexical_cast<uint16_t>(hp);
        }
        catch (boost::bad_lexical_cast) {
            throw errorx("invalid port number: %s", hp.c_str());
        }
        host = host.substr(0, pos);
    }
}

//! union of sockaddr structs
union address_u {
    sockaddr sa;
    sockaddr_in sa_in;
    sockaddr_in6 sa_in6;
    sockaddr_storage sa_stor;
};

//! wrapper around sockaddr structures that strives to
//! help make ipv6 support seemless.
struct address {
    union address_u addr;

    //! construct an empty address
    address() {
        clear();
    }

    address(int fam) {
        clear();
        addr.sa.sa_family = fam;
    }

    address(int fam, const void *a, socklen_t alen, uint16_t port_) {
        clear();
        switch (fam) {
        case AF_UNSPEC:
            break; // sure
        case AF_INET:
            if (alen != sizeof addr.sa_in.sin_addr)
                throw errorx("bad AF_INET len: %zu", (size_t)alen);
            memcpy(&addr.sa_in.sin_addr, a, alen);
            addr.sa_in.sin_port = htons(port_);
            break;
        case AF_INET6:
            if (alen != sizeof addr.sa_in6.sin6_addr)
                throw errorx("bad AF_INET6 len: %zu", (size_t)alen);
            memcpy(&addr.sa_in6.sin6_addr, a, alen);
            addr.sa_in6.sin6_port = htons(port_);
            break;
        default:
            throw errorx("bad family %d", fam);
        }
        addr.sa.sa_family = fam;
    }

    address(struct sockaddr *a, socklen_t alen) {
        clear();
        const int fam = ((struct sockaddr *)a)->sa_family;
        switch (fam) {
        case AF_UNSPEC: return;
        case AF_INET:   if (alen != sizeof addr.sa_in)  throw errorx("bad sockaddr_in len %zu",  (size_t)alen); break;
        case AF_INET6:  if (alen != sizeof addr.sa_in6) throw errorx("bad sockaddr_in6 len %zu", (size_t)alen); break;
        default:        throw errorx("bad family %d", fam);
        }
        memcpy(&addr.sa, a, alen);
    }

    //! \param s address in string format for inet_pton
    //! \param p port in host byte order
    address(const char *s, uint16_t p=0) {
        clear();
        pton(s); // can throw
        port(p);
    }

    //! \param s address in string format for inet_pton
    static address parse(const std::string &s) {
        std::string host = s;
        uint16_t port = 0;
        parse_host_port(host, port);
        return address(host.c_str(), port);
    }

    //! zeros out the address
    void clear() {
        memset(&addr, 0, sizeof addr);
    }

    //! \return sizeof(sockaddr_in) or sizeof(sockaddr_in6) depending on the value of family()
    socklen_t addrlen() const {
        switch (family()) {
        case AF_INET:   return sizeof addr.sa_in;
        case AF_INET6:  return sizeof addr.sa_in6;
        default:        return 0;
        }
    }

    //! \return maximum supported address size
    static size_t maxlen() {
        return sizeof addr.sa_stor;
    }

    //! \return a sockaddr pointer to the backing storage
    struct sockaddr *sockaddr() const {
        // cast away const cause i'm evil
        return const_cast<struct sockaddr *>(&addr.sa);
    }

    //! return AF_UNSPEC, AF_INET, or AF_INET6
    int family() const { return addr.sa.sa_family; }

    //! sets port
    //! \param p port in host by order
    void port(uint16_t p) {
        switch (family()) {
        case AF_INET:  addr.sa_in.sin_port   = htons(p); break;
        case AF_INET6: addr.sa_in6.sin6_port = htons(p); break;
        default:       throw errorx("can't set port");
        }
    }

    //! returns port in host byte order
    uint16_t port() const {
        switch (family()) {
        case AF_INET:  return ntohs(addr.sa_in.sin_port);
        case AF_INET6: return ntohs(addr.sa_in6.sin6_port);
        default:       return 0;
        }
    }

    //! wrapper around inet_ntop
    const char *ntop(char *dst, socklen_t size) const {
        const char *p;
        switch (family()) {
        case AF_INET:  p = inet_ntop(family(), &addr.sa_in.sin_addr,   dst, size); break;
        case AF_INET6: p = inet_ntop(family(), &addr.sa_in6.sin6_addr, dst, size); break;
        default:       return nullptr;
        }
        throw_if(p == nullptr);
        return p;
    }

    //! tries inet_pton() first with AF_INET and then AF_INET6
    void pton(const char *s) {
        if (inet_pton(AF_INET, s, &addr.sa_in.sin_addr) == 1) {
            addr.sa.sa_family = AF_INET;
        } else if (inet_pton(AF_INET6, s, &addr.sa_in6.sin6_addr) == 1) {
            addr.sa.sa_family = AF_INET6;
        } else {
            throw errno_error{s};
        }
    }

    std::string str() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    //! output address in addr:port format
    friend std::ostream &operator<< (std::ostream &out, const address &addr) {
        char buf[INET6_ADDRSTRLEN];
        if (addr.ntop(buf, sizeof(buf))) {
            out << buf;
            if (addr.port()) {
                out << ":" << addr.port();
            }
        }
        else {
            out << "{invalid_address}";
        }
        return out;
    }
};

} // end namespace ten

#endif // TEN_ADDRESS_HH
