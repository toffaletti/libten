#include "gtest/gtest.h"
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

TEST(Metrics, ThreadLocal) {
    using namespace metrics;

    std::vector<std::thread> threads(nthreads);
    for (auto &i : threads) {
        i = std::thread(my_thread);
    }

    {
        auto mg = global.aggregate();
        for (auto kv : mg)
            VLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(json_visitor(), kv.second);
    }

    for (auto &i : threads) {
        i.join();
    }

    auto mg = global.aggregate();

    EXPECT_EQ(nthreads, value<counter>(mg, "thing"));
    for (auto kv : mg) {
        VLOG(1) << "metric: " << kv.first << " = " << boost::apply_visitor(json_visitor(), kv.second);
    }
}

TEST(Metrics, Timer) {
    using namespace metrics;
    using namespace std::chrono;
    time_op to("timer1");
    usleep(5*1000);
    to.stop();
    auto mg = global.aggregate();
    auto v = value<timer>(mg, "timer1");
    // value type is a duration of unspecified resolution
    EXPECT_EQ(  duration_cast<seconds     >(v).count(), 0);
    EXPECT_NEAR(duration_cast<milliseconds>(v).count(), 5.0, 1.5);
    EXPECT_NEAR(duration_cast<microseconds>(v).count(), 5000.0, 1500.0);
}

TEST(Metrics, Ewma) {
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
    VLOG(1) << "rate1: "  << m1.rate()  << "/s, " << m1.rate<minutes>()  << "/m";
    VLOG(1) << "rate5: "  << m5.rate()  << "/s, " << m5.rate<minutes>()  << "/m";
    VLOG(1) << "rate60: " << m60.rate() << "/s, " << m60.rate<minutes>() << "/m";

    EXPECT_NEAR(10.0, m1.rate(), 6.0);
    EXPECT_NEAR(10.0, m5.rate(), 2.0);
    EXPECT_NEAR(10.0, m60.rate(), 1.0);

    for (unsigned i=0; i<60; ++i) {
        m1.tick();
        m5.tick();
        m60.tick();
    }

    VLOG(1) << "rate1: "  << m1.rate()  << "/s, " << m1.rate<minutes>()  << "/m";
    VLOG(1) << "rate5: "  << m5.rate()  << "/s, " << m5.rate<minutes>()  << "/m";
    VLOG(1) << "rate60: " << m60.rate() << "/s, " << m60.rate<minutes>() << "/m";

    EXPECT_NEAR(0.0, std::round(m1.rate()), 1.0);
    EXPECT_NEAR(0.0, std::round(m5.rate()), 1.0);
    EXPECT_NEAR(4.0, std::round(m60.rate()), 1.0);
}

TEST(Metrics, Interval) {
    task::main([] {
        using namespace metrics;
        using namespace std::chrono;

        // TODO: intervals internals are in seconds, else we could use e.g. 50ms here.
        constexpr seconds tick{1};
        intervals iv(tick, 2);

        // put all test events into the middle of the collection window
        this_task::sleep_for(duration_cast<milliseconds>(tick) / 2);

        std::thread bg([] {
            for (auto n : { 1, 1 }) {
                std::this_thread::sleep_for(tick);
                VLOG(1) << "* " << n << " event(s)";
                record().counter("t").incr(n);
            }
        });

        EXPECT_FALSE(iv.last(tick));

        seconds span{};
        for (auto n : { 0, 1, 2, 1, 0 }) {
            this_task::sleep_for(tick);

            VLOG(1) << "interval expects " << n << " event(s) over " << span.count() << " sec";

            // asking for more than has been collected fails
            EXPECT_FALSE(iv.last(span + tick));

            // first request is too soon, so returns nullopt
            auto im = iv.last(span);
            if (span == seconds::zero())
                EXPECT_FALSE(im);
            else  {
                EXPECT_EQ(span.count(), im->first.count());
                EXPECT_EQ(n, value<counter>(im->second, "t"));
            }

            // do not allow span to exceed collection max
            if (span < iv.duration())
                span += tick;
        }

        bg.join();
    });
}
