#ifndef LIBTEN_TASK_COMPAT_HH
#define LIBTEN_TASK_COMPAT_HH

#include <poll.h>

// XXX: old task api has been moved here to be deprecated
// all of these apis will eventially be replaced

namespace ten {

extern void netinit();

//! set/get current task state
const char *taskstate(const char *fmt=nullptr, ...);
//! set/get current task name
const char * taskname(const char *fmt=nullptr, ...);

//! suspend task waiting for io on pollfds
int taskpoll(pollfd *fds, nfds_t nfds, optional_timeout ms=nullopt);
//! suspend task waiting for io on fd
bool fdwait(int fd, int rw, optional_timeout ms=nullopt);

} // ten

#endif
