#include "ssl.hh"
#include "ioproc.hh"
#include <openssl/err.h>
#include <arpa/inet.h>

namespace ten {

static int netfd_write(BIO *b, const char *buf, int num);
static int netfd_read(BIO *b, char *buf, int size);
static int netfd_puts(BIO *b, const char *str);
static long netfd_ctrl(BIO *b, int cmd, long num, void *ptr);
static int netfd_new(BIO *b);
static int netfd_free(BIO *b);

struct netfd_state_s {
    /* fields for BIO_TYPE_CONNECT */
    char *param_hostname;
    char *param_port;
    u_int8_t ip[4];
    u_int16_t port;
    /* field for BIO_TYPE_ACCEPT */
    char *param_addr;
    BIO *bio_chain;
};
typedef struct netfd_state_s netfd_state_t;

static BIO_METHOD methods_st = {
    BIO_TYPE_SOCKET | BIO_TYPE_CONNECT | BIO_TYPE_ACCEPT,
    "state threads netfd",
    netfd_write,
    netfd_read,
    netfd_puts,
    NULL, /* gets() */
    netfd_ctrl,
    netfd_new,
    netfd_free,
    NULL,
};

BIO_METHOD *BIO_s_netfd(void) {
    return (&methods_st);
}

BIO *BIO_new_netfd(int fd, int close_flag) {
    BIO *ret = BIO_new(BIO_s_netfd());
    if (ret == NULL) return NULL;
    BIO_set_fd(ret, fd, close_flag);
    return ret;
}

static int netfd_new(BIO *b) {
    b->init = 0;
    b->num = 0;
    b->ptr = calloc(1, sizeof(netfd_state_t));
    b->flags = 0;
    return 1;
}

static void _free_netfd(BIO *b) {
    if (b == NULL) return;
    netfd_state_t *s = (netfd_state_t *)b->ptr;
    if (s == NULL) return;
    if (b->num) {
        if (b->shutdown) {
            close(b->num);
        }
    }
    if (s->param_hostname != NULL)
        OPENSSL_free(s->param_hostname);
    if (s->param_port != NULL)
        OPENSSL_free(s->param_port);
    if (s->param_addr != NULL)
        OPENSSL_free(s->param_addr);

}

static int netfd_free(BIO *b) {
    if (b == NULL) return 0;
    if (b->ptr) {
        _free_netfd(b);
        free(b->ptr);
    }
    b->ptr = NULL;
    return 1;
}

static int netfd_write(BIO *b, const char *buf, int num) {
    //netfd_state_t *s = (netfd_state_t *)b->ptr;
    return netsend(b->num, buf, num, 0, 0);
}

static int netfd_read(BIO *b, char *buf, int size) {
    //netfd_state_t *s = (netfd_state_t *)b->ptr;
    return netrecv(b->num, buf, size, 0, 0);
}

static int netfd_puts(BIO *b, const char *str) {
    //netfd_state_t *s = (netfd_state_t *)b->ptr;
    size_t n = strlen(str);
    return netsend(b->num, str, n, 0, 0);
}

static int netfd_connect(BIO *b) {
    netfd_state_t *s = (netfd_state_t *)b->ptr;
    if (s->port == 0) {
        if (BIO_get_port(s->param_port, &s->port) <= 0) {
            return -1;
        }
    }
    if (netdial(b->num, s->param_hostname, s->port) == 0) {
        // success!
        return 1;
    }

    return 0;
}

static long netfd_ctrl(BIO *b, int cmd, long num, void *ptr) {
    netfd_state_t *s = (netfd_state_t *)b->ptr;
    long ret = 1;
    int *ip;
    const char **pptr;

    switch (cmd) {
        case BIO_C_SET_FD:
            _free_netfd(b);
            b->num = *((int *)ptr);
            b->shutdown = (int)num;
            b->init = 1;
            break;
        case BIO_C_GET_FD:
            if (b->init) {
                ip = (int *)ptr;
                if (ip) *ip=b->num;
                ret = b->num;
            } else {
                ret = -1;
            }
            break;

        case BIO_CTRL_GET_CLOSE:
            ret = b->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            b->shutdown = (int)num;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        case BIO_CTRL_RESET:
            /* TODO: might need to support this for connection resets */
            break;
        case BIO_C_GET_CONNECT:
            if (ptr != NULL) {
                pptr=(const char **)ptr;
                if (num == 0) {
                    *pptr = s->param_hostname;
                } else if (num == 1) {
                    *pptr = s->param_port;
                } else if (num == 2) {
                    *pptr = (char *)&(s->ip[0]);
                } else if (num == 3) {
                    *((int *)ptr) = s->port;
                }
                if ((!b->init) || (ptr == NULL))
                    *pptr = "not initialized";
                ret = 1;
            }
            break;
        case BIO_C_SET_CONNECT:
            if (ptr != NULL)
            {
                b->init=1;
                if (num == 0) {
                    if (s->param_hostname != NULL)
                        OPENSSL_free(s->param_hostname);
                    s->param_hostname=BUF_strdup((char *)ptr);
                } else if (num == 1) {
                    if (s->param_port != NULL)
                        OPENSSL_free(s->param_port);
                    s->param_port = BUF_strdup((char *)ptr);
                } else if (num == 2) {
                    char buf[16];
                    unsigned char *p = (unsigned char *)ptr;

                    BIO_snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                                 p[0],p[1],p[2],p[3]);
                    if (s->param_hostname != NULL)
                        OPENSSL_free(s->param_hostname);
                    s->param_hostname=BUF_strdup(buf);
                    memcpy(&(s->ip[0]), ptr, 4);
                } else if (num == 3) {
                    //char buf[DECIMAL_SIZE(int)+1];
                    char buf[32];

                    BIO_snprintf(buf, sizeof(buf), "%d", (uint16_t)(intptr_t)ptr);
                    if (s->param_port != NULL)
                        OPENSSL_free(s->param_port);
                    s->param_port=BUF_strdup(buf);
                    s->port=(intptr_t)ptr;
                }
            }
            break;
        case BIO_C_SET_ACCEPT:
            if (ptr != NULL) {
                if (num == 0) {
                    b->init = 1;
                    if (s->param_addr != NULL)
                        OPENSSL_free(s->param_addr);
                    s->param_addr = BUF_strdup((char *)ptr);
                } else if (num == 1) {
                    // no blocking io with state-threads
                    /*data->accept_nbio = (ptr != NULL);*/
                } else if (num == 2) {
                    if (s->bio_chain != NULL)
                        BIO_free(s->bio_chain);
                    s->bio_chain = (BIO *)ptr;
                }
            }
            break;
        case BIO_C_DO_STATE_MACHINE:
            ret = netfd_connect(b);
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}


sslerror::sslerror() {
    err = ERR_get_error();
    ERR_error_string(err, errstr);
}


sslsock::sslsock(int fd) throw (errno_error)
    : sockbase(fd), ctx(0), bio(0)
{
}

sslsock::sslsock(int domain, int type, int protocol) throw (errno_error)
    : sockbase(domain, type | SOCK_NONBLOCK, protocol), ctx(0), bio(0)
{
}

sslsock::~sslsock() {
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
}

void sslsock::initssl(SSL_CTX *ctx_, bool client) {
    ctx = ctx_;
    BIO *ssl_bio = BIO_new_ssl(ctx, client);
    BIO *net_bio = BIO_new_netfd(s.fd, 0);
    bio = BIO_push(ssl_bio, net_bio);
}

void sslsock::initssl(const SSL_METHOD *method, bool client) {
    initssl(SSL_CTX_new(method), client);
}

int sslsock::dial(const char *addr, uint16_t port, unsigned timeout_ms) {
    // need large stack size for getaddrinfo (see dial)
    // TODO: maybe replace with c-ares from libcurl project
    ioproc io(8*1024*1024);
    int status = iodial(io, s.fd, addr, port);
    if (status != 0) return status;

    handshake();

    return 0;
}

void sslsock::handshake() {
    if (BIO_do_handshake(bio) <= 0) {
        throw sslerror();
    }
}


} // end namespace ten

