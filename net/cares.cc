#include <ares.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "net.hh"
#include "ioproc.hh"
#include "logging.hh"

namespace ten {

static __thread int _need_init = 1;
static __thread ares_channel _channel;

// convert read and write fd_set to pollfd
// max_fd pollfds will be malloced and returned in fds_p
// actual number of fds will be returned in nfds;
static void fd_sets_to_pollfd(fd_set *read_fds, fd_set *write_fds, int max_fd, struct pollfd **fds_p, int *nfds) {
    // using max_fd is over allocating
    struct pollfd *fds = (struct pollfd *)calloc(max_fd, sizeof(struct pollfd));
    int ifd = 0;
    for (int fd = 0; fd<max_fd; fd++) {
        fds[ifd].fd = fd;
        if (FD_ISSET(fd, read_fds)) {
            fds[ifd].events |= POLLIN;
        }
        if (FD_ISSET(fd, write_fds)) {
            fds[ifd].events |= POLLOUT;
        }
        // only increment the fd index if it exists in the fd sets
        if (fds[ifd].events != 0) {
            ifd++;
        }
    }
    *fds_p = fds;
    *nfds = ifd;
}

// convert pollfd to read and write fd_sets
static void pollfd_to_fd_sets(struct pollfd *fds, int nfds, fd_set *read_fds, fd_set *write_fds) {
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    for (int i = 0; i<nfds; i++) {
        if (fds[i].revents & POLLIN) {
            FD_SET(fds[i].fd, read_fds);
        }
        if (fds[i].revents & POLLOUT) {
            FD_SET(fds[i].fd, write_fds);
        }
    }
}

struct sock_info {
    const char *addr;
    int fd;
    int status;
    uint16_t port;
};

static void gethostbyname_callback(void *arg, int status, int timeouts, struct hostent *host) {
    sock_info *si = (sock_info *)arg;
    (void)timeouts; // the number of times the quest timed out during request
    si->status = status;
    if (status != ARES_SUCCESS)
    {
        DVLOG(3) << "CARES: " << ares_strerror(status);
        return;
    }

    int n = 0;
    while (host->h_addr_list && host->h_addr_list[n]) {
        address addr(host->h_addrtype, host->h_addr_list[n], host->h_length, si->port);
        status = netconnect(si->fd, addr, 0);
        if (status == 0) break;

        n++;
    }
}

int netdial(int fd, const char *addr, uint16_t port) {
    sock_info si = {addr, fd, ARES_SUCCESS, port};
    if (_need_init) {
        int status = ares_init(&_channel);
        if (status != ARES_SUCCESS) {
            ares_destroy(_channel);
            return status;
        }
        _need_init = 0;
    }

    ares_gethostbyname(_channel, addr, AF_INET, gethostbyname_callback, &si);

    fd_set read_fds, write_fds;
    struct timeval *tvp, tv;
    int max_fd, nfds;
    while (si.status == ARES_SUCCESS) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        max_fd = ares_fds(_channel, &read_fds, &write_fds);
        if (max_fd == 0)
            break;

        struct pollfd *fds;
        fd_sets_to_pollfd(&read_fds, &write_fds, max_fd, &fds, &nfds);
        std::unique_ptr<struct pollfd, void (*)(void *)> fds_p(fds, free);
        tvp = ares_timeout(_channel, NULL, &tv);
        // TODO: get timeout working
        // include tvp->tv_usec in timeout
        if (taskpoll(fds, nfds, SEC2MS(tvp->tv_sec)) == -1) {
            /* TODO: handle errors here */
        }
        pollfd_to_fd_sets(fds, nfds, &read_fds, &write_fds);
        ares_process(_channel, &read_fds, &write_fds);
    }
    return si.status;
}

void netinit() {
    // called once per process
    int status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS) {
        LOG(FATAL) << ares_strerror(status);
    }
}

} // end namespace ten
