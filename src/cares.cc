#include <ares.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ten/net.hh"
#include "ten/ioproc.hh"
#include "ten/logging.hh"

namespace ten {

struct channel_destroyer {
    ares_channel channel;
    ~channel_destroyer() {
        ares_destroy(channel);
    }
};

// convert read and write fd_set to pollfd
std::vector<pollfd> fd_sets_to_pollfd(fd_set *read_fds, fd_set *write_fds, const int max_fd) {
    std::vector<pollfd> fds;
    for (int fd = 0; fd<max_fd; fd++) {
        pollfd p = {};
        if (FD_ISSET(fd, read_fds)) {
            p.events |= EPOLLIN;
        }
        if (FD_ISSET(fd, write_fds)) {
            p.events |= EPOLLOUT;
        }
        if (p.events) {
            p.fd = fd;
            fds.push_back(p);
        }
    }
    return fds;
}

// convert pollfd to read and write fd_sets
// NOTE the caller must zero the sets first
static void pollfd_to_fd_sets(struct pollfd *fds, int nfds, fd_set *read_fds, fd_set *write_fds) {
    for (int i = 0; i<nfds; i++) {
        if (fds[i].revents & (EPOLLIN|EPOLLERR)) {
            FD_SET(fds[i].fd, read_fds);
        }
        if (fds[i].revents & (EPOLLOUT|EPOLLERR)) {
            FD_SET(fds[i].fd, write_fds);
        }
    }
}

namespace {

struct sock_info {
    const char *addr;
    uint16_t port;
    int fd;
    optional_timeout connect_ms;
    int status;
    int sys_errno;
};
constexpr int SYSTEM_ERROR = -1; // not a valid ares status

extern "C" void gethostbyname_callback(void *arg, int status, int /*timeouts*/, hostent *host) noexcept {
    sock_info * const si = reinterpret_cast<sock_info *>(arg);

    if (status != ARES_SUCCESS) {
        si->status = status;
        DVLOG(3) << "CARES: " << ares_strerror(status);
        return;
    }

    if (!host->h_addr_list || !host->h_addr_list[0]) {
        si->status = ARES_ENODATA;
        LOG(WARNING) << "BUG: c-ares returned empty address list for " << si->addr << ":" << si->port;
        return;
    }

    for (int n = 0; host->h_addr_list && host->h_addr_list[n]; ++n) {
        const address addr(host->h_addrtype, host->h_addr_list[n], host->h_length, si->port);
        if (!netconnect(si->fd, addr, {})) {
            si->status = ARES_SUCCESS;
            return;
        }
        si->status = SYSTEM_ERROR;  // not a valid ares status
        si->sys_errno = errno;
    }
}

} // anon

void netdial(int fd, const char *addr, uint16_t port, optional_timeout connect_ms)
    throw (errno_error, hostname_error)
{
    channel_destroyer cd;
    int status = ares_init(&cd.channel);
    if (status != ARES_SUCCESS) {
        throw hostname_error("unknown host %s: %s", addr, ares_strerror(status));
    }

    sock_info si = {addr, port, fd, connect_ms, ARES_SUCCESS, 0};
    ares_gethostbyname(cd.channel, addr, AF_INET, gethostbyname_callback, &si);

    // we allocate our own fd set because FD_SETSIZE is 1024
    // and we could easily have more file descriptors
    // this runs the risk of stack overflow so big stacks
    // should be used for dns lookup
    const long open_max = sysconf(_SC_OPEN_MAX);
    const size_t set_size = static_cast<size_t>((open_max + __NFDBITS - 1) / __NFDBITS);
    __fd_mask read_fd_buf[set_size];  // C99
    __fd_mask write_fd_buf[set_size]; // C99
    fd_set * const read_fds  = (fd_set *)read_fd_buf;
    fd_set * const write_fds = (fd_set *)write_fd_buf;

    while (si.status == ARES_SUCCESS) {
        memset(read_fd_buf,  0, sizeof(read_fd_buf));
        memset(write_fd_buf, 0, sizeof(write_fd_buf));
        int max_fd = ares_fds(cd.channel, read_fds, write_fds);
        if (max_fd == 0)
            break;
        auto fds = fd_sets_to_pollfd(read_fds, write_fds, max_fd);

        struct timeval *tvp, tv;
        tvp = ares_timeout(cd.channel, NULL, &tv);
        optional_timeout poll_timeout;
        if (tvp) {
            using namespace std::chrono;
            poll_timeout = duration_cast<milliseconds>(seconds(tvp->tv_sec));
        }
        taskpoll(&fds[0], fds.size(), poll_timeout);

        memset(read_fd_buf,  0, sizeof(read_fd_buf));
        memset(write_fd_buf, 0, sizeof(write_fd_buf));
        pollfd_to_fd_sets(&fds[0], fds.size(), read_fds, write_fds);
        ares_process(cd.channel, read_fds, write_fds);
    }

    if (si.status == SYSTEM_ERROR) {
        throw errno_error(si.sys_errno, "%s:%u", addr, port);
    }
    if (si.status != ARES_SUCCESS) {
        throw hostname_error("unknown host %s: %s", addr, ares_strerror(si.status));
    }
}

void netinit() {
    // called once per process
    int status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS) {
        LOG(FATAL) << ares_strerror(status);
    }
}

} // end namespace ten
