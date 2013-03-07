#include "ten/metrics.hh"

namespace ten {
namespace metrics {

global_group global;

thread_cached<metric_tag, group> tls_group;

} // metrics
} // ten
