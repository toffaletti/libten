#ifndef LIBTEN_STRIPED_HH
#define LIBTEN_STRIPED_HH

#include <functional>
#include <vector>
#include <algorithm>

namespace ten {

template <class T> class striped {
public:
    typedef typename std::vector<T>::size_type size_type;
private:
    std::vector<T> _stripes;

    template <class Key, class... Keys>
        std::vector<size_type> _mget(std::vector<size_type> &indexes, const Key key, const Keys... keys)
    {
        std::hash<Key> hasher;
        size_t hash = hasher(key);
        indexes.push_back(hash % _stripes.size());
        return _mget(indexes, keys...);
    }

    std::vector<size_type> _mget(std::vector<size_type> &indexes) {
        std::sort(begin(indexes), end(indexes));
        return indexes;
    }
public:
    striped(size_type size) : _stripes(size) {}

    template <class Key, class... Keys>
        std::vector<size_type> multi_get(const Key key, const Keys... keys)
   {
        std::vector<size_type> indexes;
        std::hash<Key> hasher;
        size_t hash = hasher(key);
        indexes.push_back(hash % _stripes.size());
        return _mget(indexes, keys...);
    }


    template <class Key> T &get(const Key &key) {
        std::hash<Key> hasher;
        size_t hash = hasher(key);
        return at(hash % _stripes.size());
    }

    T &at(const size_type index) {
        return _stripes.at(index);
    }
    
    size_type size() const {
        return _stripes.size();
    }
};

} // ten

#endif // LIBTEN_STRIPED_HH
