#define BOOST_TEST_MODULE qutex test
#include <boost/test/unit_test.hpp>
#include <thread>
#include "ten/semaphore.hh"
#include "ten/synchronized.hh"
#include "ten/task/rendez.hh"

using namespace ten;

struct state {
    qutex q;
    rendez r;
    int x;

    state() : x(0) {}
};

void qlocker(std::shared_ptr<state> st) {
    for (int i=0; i<1000; ++i) {
        std::unique_lock<qutex> lk{st->q};
        ++(st->x);
    }
    st->r.wakeup();
}

bool is_done(int &x) { return x == 20*1000; }

void qutex_task_spawn() {

    std::shared_ptr<state> st = std::make_shared<state>();
    for (int i=0; i<20; ++i) {
        procspawn(std::bind(qlocker, st));
        taskyield();
    }
    std::unique_lock<qutex> lk{st->q};
    st->r.sleep(lk, std::bind(is_done, std::ref(st->x)));
    BOOST_CHECK_EQUAL(st->x, 20*1000);
}

BOOST_AUTO_TEST_CASE(qutex_test) {
    procmain p;
    taskspawn(qutex_task_spawn);
    p.main();
}

BOOST_AUTO_TEST_CASE(sync_test) {
    synchronized<std::string> s("empty");

    synchronize(s, [](std::string &str) {
            BOOST_CHECK_EQUAL("empty", str);
            str = "test";
    });

    synchronize(s, [](std::string &str) {
            BOOST_CHECK_EQUAL("test", str);
    });
}
