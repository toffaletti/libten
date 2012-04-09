#ifndef ADDRESS_HH
#define ADDRESS_HH

#include "error.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace ten {

using std::string;
using boost::lexical_cast;

inline void parse_host_port(string &host, unsigned short &port) {
    size_t pos = host.rfind(':');
    if (pos != string::npos) {
        port = lexical_cast<unsigned short>(host.substr(pos+1, -1));
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

    address(int fam, char *a, socklen_t alen, uint16_t port) {
        family(fam);
        if (fam == AF_INET) {
            memcpy(&addr.sa_in.sin_addr, a, alen);
            addr.sa_in.sin_port = htons(port);
        } else if (fam == AF_INET6) {
            memcpy(&addr.sa_in6.sin6_addr, a, alen);
            addr.sa_in6.sin6_port = htons(port);
        }
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

    //! \param s address in string format for inet_pton
    static address parse(const std::string &s) {
        std::string host = s;
        uint16_t port = 0;
        parse_host_port(host, port);
        return address(host.c_str(), port);
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

    //! \return maximum supported address size
    static size_t maxlen() {
        return sizeof(addr.sa_stor);
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

    string str() const {
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

#endif // ADDRESS_HH

