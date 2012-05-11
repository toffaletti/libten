#ifndef LIBTEN_CONSISTENT_HASH_HH
#define LIBTEN_CONSISTENT_HASH_HH
#include <map>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace ten {

// Delimiter must not appear in the string representation of Node
// otherwise you risk hash collisions
template <typename Node, typename Data, unsigned Replicas, char Delimiter='$'>
class hash_ring {
private:
    typedef std::map<size_t, Node> map_type;

    map_type _ring;
    std::hash<std::string> _node_hash;
    std::hash<Data> _data_hash;
public:
    void add(const Node &node) {
        size_t hash;
        for (unsigned r = 0; r < Replicas; ++r) {
            std::ostringstream ss;
            ss << node << Delimiter << r;
            hash = _node_hash(ss.str());
            _ring[hash] = node;
        }
    }

    unsigned remove(const Node &node) {
        size_t hash;
        unsigned removed = 0;
        for (unsigned r = 0; r < Replicas; ++r) {
            std::ostringstream ss;
            ss << node << Delimiter << r;
            hash = _node_hash(ss.str());
            removed += _ring.erase(hash);
        }
        return removed;
    }

    const Node &get(const Data &data) const {
        if (_ring.empty()) {
            throw std::runtime_error("empty hash_ring");
        }
        size_t hash = _data_hash(data);
        // find first node >= hash
        auto it = _ring.lower_bound(hash);
        if (it == _ring.end()) {
            // wrap around to the front
            it = _ring.begin();
        }
        return it->second;
    }
};

} // ns ten
#endif
