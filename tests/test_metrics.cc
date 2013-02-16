#define BOOST_TEST_MODULE metrics test
#include <boost/test/unit_test.hpp>
#include "ten/metrics.hh"
#include <thread>
#include <atomic>

#include "ten/ewma.hh"

using namespace ten;

void my_thread() {
    metrics::record()
        .counter("thing").incr();
        ;
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

BOOST_AUTO_TEST_CASE(timer_test) {
    using namespace metrics;
    time_op to("timer1");
    usleep(5*1000); 
    to.stop();
    auto mg = metrics::global.aggregate();
    BOOST_CHECK_MESSAGE(value<timer>(mg, "timer1").count() >= 5,
            "timer1 = " << value<timer>(mg, "timer1").count()
            );
}

BOOST_AUTO_TEST_CASE(ewma_test) {
    using namespace std::chrono;
    ewma<seconds> m1(seconds{1});
    ewma<seconds> m5(seconds{5});
    ewma<seconds> m60(seconds{60});
    for (unsigned i=0; i<60*5; ++i) {
        m1.update(10);
        m1.tick();
        m5.update(10);
        m5.tick();
        m60.update(10);
        m60.tick();
    }
    LOG(INFO) << "rate1: "  << m1.rate()  << "/s, " << m1.rate<minutes>()  << "/m";
    LOG(INFO) << "rate5: "  << m5.rate()  << "/s, " << m5.rate<minutes>()  << "/m";
    LOG(INFO) << "rate60: " << m60.rate() << "/s, " << m60.rate<minutes>() << "/m";
}

