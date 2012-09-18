#define BOOST_TEST_MODULE metrics test
#include <boost/test/unit_test.hpp>
#include "ten/metrics.hh"
#include <thread>
#include <atomic>

using namespace ten;

void my_thread() {
    metrics::record([](metrics::metric_group &g) {
        g.update<metrics::counter>("thing").incr();
    });
}

static const int nthreads = 100;

BOOST_AUTO_TEST_CASE(thread_local_test) {
    std::vector<std::thread> threads(nthreads);
    for (auto &i : threads) {
        i = std::move(std::thread(my_thread));
    }

    {
        auto mg = metrics::global.aggregate();
        for (auto kv : mg) {
            DVLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(metrics::json_visitor(), kv.second);
        }
    }

    for (auto &i : threads) {
        i.join();
    }

    auto mg = metrics::global.aggregate();

    BOOST_CHECK_EQUAL(nthreads, metrics::value<metrics::counter>(mg, "thing"));
    for (auto kv : mg) {
        DVLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(metrics::json_visitor(), kv.second);
    }
}

