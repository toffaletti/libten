#ifndef LIBTEN_NET_SSL_HH
#define LIBTEN_NET_SSL_HH

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include "ten/net.hh"
#include "ten/error.hh"

namespace ten {

struct sslerror : public backtrace_exception {
    char errstr[128];
    long err;

    sslerror();
    const char *what() const noexcept override { return errstr; }
};

BIO_METHOD *BIO_s_netfd(void);
BIO *BIO_new_netfd(int fd, int close_flag);

//! task io aware SSL wrapper
class sslsock : public sockbase {
public:
    SSL_CTX *ctx = nullptr;
    BIO *bio = nullptr;

    sslsock(int fd=-1);
    sslsock(netsock ns);
    sslsock(int domain, int type, int protocol=0);

    sslsock(sslsock &&other) = default;
    sslsock & operator = (sslsock &&other) = default;

    ~sslsock() override;

    //! false for server mode
    void initssl(SSL_CTX *ctx_, bool client);
    void initssl(const SSL_METHOD *method, bool client);

    //! dial requires a large 8MB stack size for getaddrinfo
    void dial(const char *addr,
            uint16_t port,
            optional_timeout timeout_ms=nullopt) override;

    int connect(const address &addr,
            optional_timeout timeout_ms=nullopt) override
        __attribute__((warn_unused_result))
    {
        return netconnect(s.fd, addr, timeout_ms);
    }

    int accept(address &addr,
            int flags=0, optional_timeout timeout_ms=nullopt) override
        __attribute__((warn_unused_result))
    {
        return netaccept(s.fd, addr, flags, timeout_ms);
    }

    ssize_t recv(void *buf,
            size_t len, int flags=0, optional_timeout timeout_ms=nullopt) override
        __attribute__((warn_unused_result))
    {
        return BIO_read(bio, buf, len);
    }

    ssize_t send(const void *buf,
            size_t len, int flags=0, optional_timeout timeout_ms=nullopt) override
        __attribute__((warn_unused_result))
    {
        return BIO_write(bio, buf, len);
    }

    void handshake();

};

} // end namespace ten

#endif // LIBTEN_NET_SSL_HH
