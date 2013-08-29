#include "gtest/gtest.h"
#include <unordered_map>
#include "ten/consistent_hash.hh"
#include "ten/logging.hh"

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

TEST(HashRing, Basic) {
    std::unordered_map<std::string, server> servers = {
        {"server1.example.com", server()},
        {"server2.example.com", server()},
        {"server3.example.com", server()},
    };

    hash_ring<std::string, std::string, 3> ring;
    for (auto it : servers) {
        ring.add(it.first);
    }

    for (auto it : data) {
        auto host = ring.get(it.first);
        VLOG(1) << "storing " << it.first << "=" << it.second << " on " << host;
        servers[host].put(it.first, it.second);
    }

    for (auto it : data) {
        auto h = ring.get(it.first);
        auto d = servers[h].get(it.first);
        EXPECT_EQ(it.second, d);
    }

    // remove a server
    EXPECT_EQ(3u, ring.remove("server3.example.com"));

    unsigned found = 0;
    for (auto it : data) {
        auto h = ring.get(it.first);
        auto d = servers[h].get(it.first);
        if (it.second == d) {
            ++found;
        }
    }
    // more than 30% of the keys should still be found
    // after removing one server
    EXPECT_TRUE(found / (float)data.size() > 0.30)
        << "Found " << found << " data size: " << data.size();
}

TEST(HashRing, Remove) {
    hash_ring<std::string, std::string, 100> ring;
    ring.add("test1");
    ring.add("test2");
    ring.add("test3");
    EXPECT_EQ(100u, ring.remove("test1"));
    EXPECT_EQ(100u, ring.remove("test3"));
    EXPECT_EQ(100u, ring.remove("test2"));
}

