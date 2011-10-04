#include "task.hh"
#include <stdarg.h>
#include <memory>

namespace fw {

typedef intptr_t (*op_func)(va_list&);
typedef std::shared_ptr<struct _ioproc> iop;

iop ioproc(size_t stacksize=default_stacksize);
intptr_t iocall(iop &io, const op_func &op, ...);

int ioopen(iop &io, char *path, int mode);
int ioclose(iop &io, int fd);
ssize_t ioread(iop &io, int fd, void *buf, size_t n);
ssize_t iowrite(iop &io, int fd, void *a, size_t n);

int iodial(iop &io, int fd, const char *addr, uint16_t port);

} // end namespace fw
