#if (__GNUC__ >= 4 && (__GNUC_MINOR__ > 4))
#include <atomic>
#else
#include <stdatomic.h>
#endif
