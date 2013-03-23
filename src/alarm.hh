#ifndef TEN_TASK_ALARM_HH
#define TEN_TASK_ALARM_HH

#include <chrono>
#include <vector>
#include <algorithm>
#include "ten/optional.hh"
#include "ten/ptr.hh"

namespace ten {

template <class T, class Clock>
struct alarm_clock {
    typedef std::chrono::time_point<Clock> time_point;

private:
    struct data {
        T value;
        time_point when;
        std::exception_ptr exception;

        bool operator == (const data &other) {
            if (value == other.value
                    && when == other.when
                    && exception == other.exception)
            {
                return true;
            }
            return false;
        }
    };

    typedef std::vector<data> container_type;
    container_type _set;

    struct order {
        bool operator ()(const data &a, const data &b) const {
            return a.when < b.when;
        }
    };

    data insert(const T &t, const time_point &when) {
        data d{t, when};
        auto i = std::lower_bound(std::begin(_set),
                std::end(_set), d, order());
        _set.insert(i, d);
        return d;
    }

    template<typename Exception>
    data insert(const T &t, const time_point &when, Exception e) {
        data d{t, when, std::make_exception_ptr(e)};
        auto i = std::lower_bound(std::begin(_set),
                std::end(_set), d, order());
        _set.insert(i, d);
        return d;
    }

    void remove(const data &d) {
        auto i = find(begin(_set), end(_set), d);
        if (i != end(_set)) {
            _set.erase(i);
        }
    }

    void remove(const T &value) {
        auto i = remove_if(begin(_set), end(_set),
                [&value](const data &d) { return d.value == value; });
        _set.erase(i, end(_set));
    }

public:
    template <class Function>
    void tick(const time_point &now, Function f) {
        auto i = begin(_set);
        for (; i != end(_set); ++i) {
            if (i->when <= now) {
                f(i->value, i->exception);
            } else {
                break;
            }
        }
        _set.erase(begin(_set), i);
    }

    bool empty() const { return _set.empty(); }

    optional<time_point> when() const {
        if (_set.empty()) {
            return nullopt;
        }
        return _set.front().when;
    }

public:
    struct scoped_alarm {

        ptr<alarm_clock<T, Clock>> _set;
        typename alarm_clock<T, Clock>::data _data;
        bool _armed = false;

        scoped_alarm() {}

        scoped_alarm(const scoped_alarm &) = delete;
        scoped_alarm &operator = (const scoped_alarm &) = delete;

        scoped_alarm(scoped_alarm &&other) : _set{other._set} {
            other._set.reset();
            std::swap(_data, other._data);
            std::swap(_armed, other._armed);
        }

        scoped_alarm &operator = (scoped_alarm &&other) {
            if (this != &other) {
                std::swap(_set, other._set);
                std::swap(_data, other._data);
                std::swap(_armed, other._armed);
            }
            return *this;
        }

        scoped_alarm(alarm_clock<T, Clock> &s, const T &value, time_point when)
            : _set(&s), _armed(true)
        {
            _data = _set->insert(value, when);
        }

        template <class Exception>
            scoped_alarm(alarm_clock<T, Clock> &s, const T &value, time_point when, Exception e)
            : _set(&s), _armed(true)
            {
                _data = _set->insert(value, when, e);
            }

        std::chrono::milliseconds remaining() const {
            using namespace std::chrono;
            if (_armed) {
                time_point now = Clock::now();
                if (now > _data.when) {
                    return milliseconds{0};
                }
                return duration_cast<milliseconds>(_data.when - now);
            }
            return milliseconds{0};
        }

        void cancel() {
            if (_armed) {
                _armed = false;
                _set->remove(_data);
            }
        }

        ~scoped_alarm() {
            cancel();
        }
    };

};

} // ten

#endif
