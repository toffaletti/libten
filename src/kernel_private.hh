#ifndef TEN_KERNEL_PRIVATE_HH
#define TEN_KERNEL_PRIVATE_HH

#include "ten/task/kernel.hh"
#include "task_impl.hh"
#include "ten/ptr.hh"

namespace ten {
namespace kernel {

ptr<task::impl> current_task();

} // kernel
} // ten

#endif
