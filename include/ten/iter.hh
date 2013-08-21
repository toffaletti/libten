#ifndef LIBTEN_ITER_HH
#define LIBTEN_ITER_HH

#include <iterator>

namespace ten {

// cyclic iterator

template <class Iter>
class cycle_iterator {
    using itr = typename std::iterator_traits<Iter>;

    const Iter from, to;
    Iter cur;

public:
    using value_type        = typename itr::value_type;
    using pointer           = typename itr::pointer;
    using reference         = typename itr::reference;
    using iterator_category = std::forward_iterator_tag;

    cycle_iterator(Iter from_, Iter to_) : from(std::move(from_)), to(std::move(to_)), cur(from) {}

    bool empty() const {
        return from == to;
    }
    reference operator * () const {
        if (empty()) throw std::out_of_range("empty cycle_iterator");
        return *cur;
    }
    cycle_iterator & operator ++ () {
        if (empty()) throw std::out_of_range("empty cycle_iterator");
        if (++cur == to)
            cur = from;
        return *this;
    }
    cycle_iterator operator ++ (int) {
        cycle_iterator orig(*this);
        ++*this;
        return orig;
    }
};

template <class T, class Iter = typename T::iterator>
  inline cycle_iterator<Iter> make_cycle_iterator(T &coll)       { return cycle_iterator<Iter>(begin(coll), end(coll)); }

template <class T, class Iter = typename T::const_iterator>
  inline cycle_iterator<Iter> make_cycle_iterator(const T &coll) { return cycle_iterator<Iter>(begin(coll), end(coll)); }

template <class T, class RRef = T&&>
  void make_cycle_iterator(RRef);  // that should prevent using temporaries

} // ten

#endif // LIBTEN_ITER_HH
