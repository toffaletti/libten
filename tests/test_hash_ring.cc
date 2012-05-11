#define BOOST_TEST_MODULE hash_ring test
#include <unordered_map>
#include <boost/test/unit_test.hpp>
#include "ten/consistent_hash.hh"

using namespace ten;

struct server {
    std::unordered_map<std::string, std::string> cache;

    void put(const std::string &key, const std::string &value) {
        cache[key] = value;
    }

    std::string get(const std::string &key) const {
        std::string value;
        auto it = cache.find(key);
        if (it != cache.end()) {
            value = it->second;
        }
        return value;
    }
};

static const std::map<std::string, std::string> data = {
    {"zero",  "0"},
    {"one",   "1"},
    {"two",   "2"},
    {"three", "3"},
    {"four",  "4"},
    {"five",  "5"},
    {"six",   "6"},
    {"seven", "7"},
    {"eight", "8"},
    {"nine",  "9"},
    {"ten",   "10"},
};

BOOST_AUTO_TEST_CASE(hash_ring_basic_test) {
    std::unordered_map<std::string, server> servers = {
        {"server1.example.com", server()},
        {"server2.example.com", server()},
        {"server3.example.com", server()},
    };

    hash_ring<std::string, std::string, 1> ring;
    for (auto it : servers) {
        ring.add(it.first);
    }

    for (auto it : data) {
        auto host = ring.get(it.first);
        BOOST_MESSAGE("storing " << it.first << "=" << it.second << " on " << host);
        servers[host].put(it.first, it.second);
    }

    for (auto it : data) {
        auto host = ring.get(it.first);
        auto data = servers[host].get(it.first);
        BOOST_REQUIRE_EQUAL(it.second, data);
    }

    // remove a server
    BOOST_REQUIRE_EQUAL(1, ring.remove("server3.example.com"));

    unsigned found = 0;
    for (auto it : data) {
        auto host = ring.get(it.first);
        auto data = servers[host].get(it.first);
        if (it.second == data) {
            ++found;
        }
    }
    // more than 30% of the keys should still be found
    // after removing one server
    BOOST_REQUIRE(found / (float)data.size() > 0.30);
}

BOOST_AUTO_TEST_CASE(hash_ring_remove_test) {
    hash_ring<std::string, std::string, 100> ring;
    ring.add("test1");
    ring.add("test2");
    ring.add("test3");
    BOOST_REQUIRE_EQUAL(100, ring.remove("test1"));
    BOOST_REQUIRE_EQUAL(100, ring.remove("test3"));
    BOOST_REQUIRE_EQUAL(100, ring.remove("test2"));
}

