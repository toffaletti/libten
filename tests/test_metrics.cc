#define BOOST_TEST_MODULE metrics test
#include <boost/test/unit_test.hpp>
#include "ten/metrics.hh"
#include "ten/task.hh"
#include <thread>
#include <atomic>

#include "ten/ewma.hh"

using namespace ten;

void my_thread() {
#if 1
    metrics::record()
        //.incr<metrics::counter>("thing")
        .counter("thing").incr();
        ;
#else
    metrics::record([](metrics::group &g) {
        get<metrics::counter>(g, "thing").incr();
    });
#endif
}

static const int nthreads = 100;

BOOST_AUTO_TEST_CASE(ten_init) {
    kernel::boot(); // init logging etc
}

BOOST_AUTO_TEST_CASE(thread_local_test) {
    using namespace metrics;

    std::vector<std::thread> threads(nthreads);
    for (auto &i : threads) {
        i = std::thread(my_thread);
    }

    {
        auto mg = global.aggregate();
        for (auto kv : mg)
            DVLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(json_visitor(), kv.second);
    }

    for (auto &i : threads) {
        i.join();
    }

    auto mg = global.aggregate();

    BOOST_CHECK_EQUAL(nthreads, value<counter>(mg, "thing"));
    for (auto kv : mg) {
        DVLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(json_visitor(), kv.second);
    }
}

BOOST_AUTO_TEST_CASE(timer_test) {
    using namespace metrics;
    using namespace std::chrono;
    time_op to("timer1");
    usleep(5*1000); 
    to.stop();
    auto mg = global.aggregate();
    auto v = value<timer>(mg, "timer1");
    // value type is a duration of unspecified resolution
    BOOST_CHECK(duration_cast<seconds     >(v).count() == 0);
    BOOST_CHECK(duration_cast<milliseconds>(v).count() >= 5);
    BOOST_CHECK(duration_cast<microseconds>(v).count() >= 5000);
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

    BOOST_CHECK_CLOSE(10.0, m1.rate(), 60.0);
    BOOST_CHECK_CLOSE(10.0, m5.rate(), 11.0);
    BOOST_CHECK_CLOSE(10.0, m60.rate(), 2.0);

    for (unsigned i=0; i<60; ++i) {
        m1.tick();
        m5.tick();
        m60.tick();
    }

    LOG(INFO) << "rate1: "  << m1.rate()  << "/s, " << m1.rate<minutes>()  << "/m";
    LOG(INFO) << "rate5: "  << m5.rate()  << "/s, " << m5.rate<minutes>()  << "/m";
    LOG(INFO) << "rate60: " << m60.rate() << "/s, " << m60.rate<minutes>() << "/m";

    BOOST_CHECK_CLOSE(0.0, std::round(m1.rate()), 1.0);
    BOOST_CHECK_CLOSE(0.0, std::round(m5.rate()), 1.0);
    BOOST_CHECK_CLOSE(4.0, std::round(m60.rate()), 1.0);
}

