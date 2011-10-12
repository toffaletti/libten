#include "task.hh"
#include <stdarg.h>
#include <memory>
#include "shared_pool.hh"

namespace fw {

typedef intptr_t (*op_func)(va_list);

namespace detail {
    struct ioproc;

    int ioopen(ioproc *io, char *path, int mode);
    int ioclose(ioproc *io, int fd);
    ssize_t ioread(ioproc *io, int fd, void *buf, size_t n);
    ssize_t iowrite(ioproc *io, int fd, void *a, size_t n);

    int iodial(ioproc *io, int fd, const char *addr, uint16_t port);
}

typedef std::shared_ptr<detail::ioproc> shared_ioproc;

shared_ioproc ioproc(size_t stacksize=default_stacksize);

intptr_t iocall(detail::ioproc *io, const op_func &op, ...);

template<typename T>
int ioopen(T &io, char *path, int mode) {
    return detail::ioopen(io.get(), path, mode);
}

template<typename T>
int ioclose(T &io, int fd) {
    return detail::ioclose(io.get(), fd);
}

template<typename T>
ssize_t ioread(T &io, int fd, void *buf, size_t n) {
    return detail::ioread(io.get(), fd, buf, n);
}

template<typename T>
ssize_t iowrite(T &io, int fd, void *buf, size_t n) {
    return detail::iowrite(io.get(), fd, buf, n);
}

template<typename T>
int iodial(T &io, int fd, const char *addr, uint16_t port) {
    return detail::iodial(io.get(), fd, addr, port);
}

class ioproc_pool : public shared_pool<detail::ioproc> {
public:
    ioproc_pool(size_t stacksize_=default_stacksize, ssize_t max_=-1)
        : shared_pool<detail::ioproc>("ioproc_pool", max_), stacksize(stacksize_) {}
protected:
    size_t stacksize;
    detail::ioproc *new_resource();
    void free_resource(detail::ioproc *p);
};

} // end namespace fw
