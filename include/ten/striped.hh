#ifndef LIBTEN_STRIPED_HH
#define LIBTEN_STRIPED_HH

#include <functional>
#include <vector>

namespace ten {

template <class T> class striped {
private:
    std::vector<T> _stripes;

public:
    striped(size_t size) : _stripes{size} {}

    template <class Key> T &get(const Key &key) {
        std::hash<Key> hasher;
        size_t hash = hasher(key);
        return at(hash % _stripes.size());
    }

    T &at(const size_t index) {
        return _stripes.at(index);
    }
    
    size_t size() const {
        return _stripes.size();
    }
};

} // ten

#endif // LIBTEN_STRIPED_HH
