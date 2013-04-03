#ifndef LIBTEN_ITER_HH
#define LIBTEN_ITER_HH

#include <iterator>

namespace ten {

// cyclic iterator

template <class Iter>
class cycle_iter {
    using itr = typename std::iterator_traits<Iter>;

    const Iter from, to;
    Iter cur;

public:
    using value_type        = typename itr::value_type;
    using pointer           = typename itr::pointer;
    using reference         = typename itr::reference;
    using iterator_category = std::forward_iterator_tag;

    cycle_iter(Iter from_, Iter to_) : from(std::move(from_)), to(std::move(to_)), cur(from) {}

    bool empty() const {
        return from == to;
    }
    reference operator * () const {
        if (empty()) throw std::out_of_range("empty cycle_iter");
        return *cur;
    }
    cycle_iter & operator ++ () {
        if (empty()) throw std::out_of_range("empty cycle_iter");
        if (++cur == to)
            cur = from;
        return *this;
    }
    cycle_iter operator ++ (int) {
        cycle_iter orig(*this);
        ++*this;
        return orig;
    }
};

template <class T, class Iter = typename T::iterator>
  inline cycle_iter<Iter> make_cycle_iter(T &coll)       { return cycle_iter<Iter>(begin(coll), end(coll)); }

template <class T, class Iter = typename T::const_iterator>
  inline cycle_iter<Iter> make_cycle_iter(const T &coll) { return cycle_iter<Iter>(begin(coll), end(coll)); }

} // ten

#endif // LIBTEN_ITER_HH
