#include "ten/metrics.hh"

namespace ten {
namespace metrics {

metric_global global;

thread_cached<metric_tag, metric_group> tls_metric_group;

} // metrics
} // ten
