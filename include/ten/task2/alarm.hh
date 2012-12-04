#include <chrono>
#include <deque>

namespace ten {

template <class T, class Clock>
struct alarm_set {
public:
    typedef std::chrono::time_point<Clock> time_point;
    struct data {
        T value;
        time_point when;
        std::exception_ptr exception;

        bool operator == (const data &other) {
            if (value == other.value) {
                if (when == other.when) {
                    return true;
                }
            }
            return false;
        }
    };

    typedef std::vector<data> container_type;
private:
    container_type _set;

    struct order {
        bool operator ()(const data &a, const data &b) const {
            return a.when < b.when;
        }
    };

    template<typename Exception>
    data insert(const T &t, const time_point &when, Exception e) {
        data d{t, when, std::copy_exception(e)};
        auto i = std::lower_bound(std::begin(_set),
                std::end(_set), d, order());
        _set.insert(i, d);
        return d;
    }

    void remove(const data &d) {
        using namespace std;
        auto i = find(begin(_set), end(_set), d);
        if (i != end(_set)) {
            _set.erase(i);
        }
    }

    void remove(const T &value) {
        using namespace std;
        auto i = remove_if(begin(_set), end(_set),
                [&value](const data &d) { return d.value == value; });
        _set.erase(i, end(_set));
    }

public:
    template <class Function>
    void tick(const time_point &now, Function f) {
        using namespace std;
        auto i = begin(_set);
        for (; i != end(_set); ++i) {
            if (i->when <= now) {
                f(i->value);
            } else {
                break;
            }
        }
        _set.erase(begin(_set), i);
    }

    bool empty() const { return _set.empty(); }

public:
    struct alarm {

        alarm_set<T, Clock> &_set;
        typename alarm_set<T, Clock>::data _data;
        bool _armed = true;

        alarm(alarm_set<T, Clock> &s, T value, time_point when)
            : _set(s)
        {
            _data = _set.insert(std::forward<T>(value), when, nullptr);
        }

        template <class Exception>
        alarm(alarm_set<T, Clock> &s, T value, time_point when, Exception e)
            : _set(s)
        {
            _data = _set.insert(std::forward<T>(value), when, e);
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
                _set.remove(_data);
            }
        }

        ~alarm() {
            cancel();
        }
    };
};



} // ten
