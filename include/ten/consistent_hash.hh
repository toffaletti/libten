#ifndef LIBTEN_CONSISTENT_HASH_HH
#define LIBTEN_CONSISTENT_HASH_HH
#include <map>
#include <functional>
#include <stdexcept>
#include <set>
#include <boost/lexical_cast.hpp>

namespace ten {

class hash_ring {
private:
    typedef std::map<size_t, std::string> map_type;

    unsigned _replicas;
    map_type _ring;
    std::hash<std::string> _hash;
public:
    hash_ring(unsigned replicas) : _replicas(replicas) {}

    void add(const std::string &node, unsigned replicas=0) {
        if (replicas == 0) replicas = _replicas;
        size_t hash;
        for (unsigned r = 0; r < replicas; ++r) {
            hash = _hash(node + boost::lexical_cast<std::string>(r));
            _ring[hash] = node;
        }
    }

    unsigned erase(const std::string &node, unsigned replicas=0) {
        if (replicas == 0) replicas = _replicas;
        size_t hash;
        unsigned found = 0;
        for (unsigned r = 0; r < replicas; ++r) {
            hash = _hash(node + boost::lexical_cast<std::string>(r));
            found += _ring.erase(hash);
        }
        // return number of replicas erased
        return found;
    }

    const std::string &get(const std::string &data) const {
        if (_ring.empty()) {
            throw std::runtime_error("empty hash_ring");
        }
        size_t hash = _hash(data);
        // find first node >= hash
        auto it = _ring.lower_bound(hash);
        if (it == _ring.end()) {
            // wrap around to the front
            it = _ring.begin();
        }
        return it->second;
    }

    //! unique set of nodes
    std::set<std::string> nodes() const {
        std::set<std::string> nodes;
        for (auto it : _ring) {
            nodes.insert(it.second);
        }
        return nodes;
    }
};

} // ns ten
#endif
