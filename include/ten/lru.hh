#ifndef LRU_HH
#define LRU_HH

#include <unordered_map>
#include <list>
#include <ostream>

namespace ten {

//! bounded mapping container that ages out least recently used items
template <typename Key, typename Mapped>
class lru {
public:
    typedef Key key_type;
    typedef std::pair<Key const, Mapped> value_type;
    typedef Mapped map_type;
    typedef std::list<value_type> cache_list;
    typedef std::unordered_map<Key, typename cache_list::iterator> cache_map;
    typedef typename cache_list::iterator iterator;

private:
    cache_list _list; // stores items sorted by MRU -> LRU
    cache_map _map; // stores key => iterator pointer to list
    size_t _entries; // keep our own constant time size for _list
    size_t _limit; // max number of entries

public:
    lru(size_t limit) : _map(limit*2), _entries(0), _limit(limit) { }

    // will update lru if key is found
    iterator find(key_type const &k) {
        typename cache_map::iterator i = _map.find(k);
        if (i!=_map.end()) {
            _list.push_front(*i->second);
            typename cache_list::iterator old = i->second;
            // assign mapped iterator to new begin() iterator (move to front)
            i->second = _list.begin();
            // remove from old position in list
            _list.erase(old);
            return i->second;
        }
        return _list.end();
    }

    iterator begin() { return _list.begin(); }
    iterator end() { return _list.end(); }

    std::pair <iterator, bool> insert(value_type const &v) {
        _list.push_front(v);
        std::pair<typename cache_map::iterator, bool> i =
            _map.insert(std::make_pair(v.first, _list.begin()));
        if (i.second) {
            // successful insert
            if (_entries >= _limit) {
                // expire last used entry
                _map.erase(_list.back().first);
                _list.pop_back();
            } else {
                _entries++;
            }
        } else {
            // duplicate key found
            typename cache_list::iterator old = i.first->second;
            // assign mapped iterator to new begin() iterator (move to front)
            i.first->second = _list.begin();
            // remove from old position in list
            _list.erase(old);
        }
        return std::make_pair(_list.begin(), i.second);
    }

    size_t erase(key_type const &k) {
        typename cache_map::iterator i = _map.find(k);
        if (i!=_map.end()) {
            _list.erase(i->second);
            _map.erase(i);
            return 1;
        }
        return 0;
    }

    friend std::ostream &operator <<(std::ostream &o, const lru &l) {
        // output list
        {
            typename cache_list::const_iterator i = l._list.begin();
            o << "[";
            while (i!=l._list.end()) {
                o << i->first;
                i++;
                if (i != l._list.end()) { o << ", "; }
            }
            o << "] ";
        }

        // output map
        {
            o << "{";
            typename cache_map::const_iterator i = l._map.begin();
            while (i!=l._map.end()) {
                o << i->first << ":" << i->second->second;
                if (i != l._map.end()) { o << ", "; }
                i++;
            }
            o << "}";
        }

        return o;
    }

    size_t capacity() const { return _limit; }
    size_t size() const { return _entries; }

};

} // namespace
#endif
