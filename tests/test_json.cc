#define BOOST_TEST_MODULE json test
#include <boost/test/unit_test.hpp>
#include <ten/json.hh>
#ifdef TEN_JSON_CXX11
#include <ten/jserial.hh>
#include <ten/jserial_maybe.hh>
#include <ten/jserial_seq.hh>
#include <ten/jserial_assoc.hh>
#include <ten/jserial_enum.hh>
#endif
#include <array>

#include "ten/logging.hh"
#include "ten/jsonstream.hh"

using namespace std;
using namespace ten;

const char json_text[] =
"{ \"store\": {"
"    \"book\": ["
"      { \"category\": \"reference\","
"        \"author\": \"Nigel Rees\","
"        \"title\": \"Sayings of the Century\","
"        \"price\": 8.95"
"      },"
"      { \"category\": \"fiction\","
"        \"author\": \"Evelyn Waugh\","
"        \"title\": \"Sword of Honour\","
"        \"price\": 12.99"
"      },"
"      { \"category\": \"fiction\","
"        \"author\": \"Herman Melville\","
"        \"title\": \"Moby Dick\","
"        \"isbn\": \"0-553-21311-3\","
"        \"price\": 8.99"
"      },"
"      { \"category\": \"fiction\","
"        \"author\": \"J. R. R. Tolkien\","
"        \"title\": \"The Lord of the Rings\","
"        \"isbn\": \"0-395-19395-8\","
"        \"price\": 22.99"
"      }"
"    ],"
"    \"bicycle\": {"
"      \"color\": \"red\","
"      \"price\": 19.95"
"    }"
"  }"
"}";


BOOST_AUTO_TEST_CASE(json_test_path1) {
    json o{json::load(json_text)};
    BOOST_REQUIRE(o.get());

    static const char a1[] = "[\"Nigel Rees\", \"Evelyn Waugh\", \"Herman Melville\", \"J. R. R. Tolkien\"]";
    json r1{o.path("/store/book/author")};
    BOOST_CHECK_EQUAL(json::load(a1), r1);

    json r2{o.path("//author")};
    BOOST_CHECK_EQUAL(json::load(a1), r2);

    // jansson hashtable uses size_t for hash
    // we think this is causing the buckets to change on 32bit vs. 64bit
#if (__SIZEOF_SIZE_T__ == 4)
    static const char a3[] = "[{\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}, {\"color\": \"red\", \"price\": 19.95}]";
#elif (__SIZEOF_SIZE_T__ == 8)
    static const char a3[] = "[{\"color\": \"red\", \"price\": 19.95}, {\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";
#endif
    json r3{o.path("/store/*")};
    json t3{json::load(a3)};
    BOOST_CHECK_EQUAL(t3, r3);

#if (__SIZEOF_SIZE_T__ == 4)
    static const char a4[] = "[8.95, 12.99, 8.99, 22.99, 19.95]";
#elif (__SIZEOF_SIZE_T__ == 8)
    static const char a4[] = "[19.95, 8.95, 12.99, 8.99, 22.99]";
#endif
    json r4{o.path("/store//price")};
    BOOST_CHECK_EQUAL(json::load(a4), r4);

    static const char a5[] = "{\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}";
    json r5{o.path("//book[3]")};
    BOOST_CHECK_EQUAL(json::load(a5), r5);

    static const char a6[] = "\"J. R. R. Tolkien\"";
    json r6{o.path("/store/book[3]/author")};
    BOOST_CHECK_EQUAL(json::load(a6), r6);
    BOOST_CHECK(json::load(a6) == r6);

    static const char a7[] = "[{\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";
    json r7{o.path("/store/book[category=\"fiction\"]")};
    BOOST_CHECK_EQUAL(json::load(a7), r7);
}

BOOST_AUTO_TEST_CASE(json_test_path2) {
    json o{json::load("[{\"type\": 0}, {\"type\": 1}]")};
    BOOST_CHECK_EQUAL(json::load("[{\"type\":1}]"), o.path("/[type=1]"));
}

BOOST_AUTO_TEST_CASE(json_test_filter_key_exists) {
    json o{json::load(json_text)};
    BOOST_REQUIRE(o.get());

    static const char a[] = "["
"      { \"category\": \"fiction\","
"        \"author\": \"Herman Melville\","
"        \"title\": \"Moby Dick\","
"        \"isbn\": \"0-553-21311-3\","
"        \"price\": 8.99"
"      },"
"      { \"category\": \"fiction\","
"        \"author\": \"J. R. R. Tolkien\","
"        \"title\": \"The Lord of the Rings\","
"        \"isbn\": \"0-395-19395-8\","
"        \"price\": 22.99"
"      }]";
    json r{o.path("//book[isbn]")};
    BOOST_CHECK_EQUAL(json::load(a), r);

    json r1{o.path("//book[doesnotexist]")};
    BOOST_REQUIRE(r1.is_array());
    BOOST_CHECK_EQUAL(0, r1.asize());
}

BOOST_AUTO_TEST_CASE(json_test_truth) {
    json o{{}}; // empty init list
    BOOST_CHECK(o.get("nothing").is_true() == false);
    BOOST_CHECK(o.get("nothing").is_false() == false);
    BOOST_CHECK(o.get("nothing").is_null() == false);
    BOOST_CHECK(!o.get("nothing"));
}

BOOST_AUTO_TEST_CASE(json_test_path3) {
    json o{json::load(json_text)};
    BOOST_REQUIRE(o.get());

    BOOST_CHECK_EQUAL(o, o.path("/"));
    BOOST_CHECK_EQUAL("Sayings of the Century",
        o.path("/store/book[category=\"reference\"]/title"));

    static const char text[] = "["
    "{\"type\":\"a\", \"value\":0},"
    "{\"type\":\"b\", \"value\":1},"
    "{\"type\":\"c\", \"value\":2},"
    "{\"type\":\"c\", \"value\":3}"
    "]";

    BOOST_CHECK_EQUAL(json(1), json::load(text).path("/[type=\"b\"]/value"));
}

BOOST_AUTO_TEST_CASE(json_init_list) {
    json meta{
        { "foo", 17 },
        { "bar", 23 },
        { "baz", true },
        { "corge", json::array({ 1, 3.14159 }) },
        { "grault", json::array({ "hello", string("world") }) },
    };
    BOOST_REQUIRE(meta);
    BOOST_REQUIRE(meta.is_object());
    BOOST_CHECK_EQUAL(meta.osize(), 5);
    BOOST_CHECK_EQUAL(meta["foo"].integer(), 17);
    BOOST_CHECK_EQUAL(meta["corge"][0].integer(), 1);
    BOOST_CHECK_EQUAL(meta["grault"][1].str(), "world");
}

template <class T>
inline void test_conv(T val, json j, json_type t) {
    json j2 = to_json(val);
    BOOST_CHECK_EQUAL(j2.type(), t);
    BOOST_CHECK_EQUAL(j, j2);
    T val2 = json_cast<T>(j2);
    BOOST_CHECK_EQUAL(val, val2);
}

template <class T, json_type TYPE = JSON_INTEGER>
inline void test_conv_num() {
    typedef numeric_limits<T> lim;
    T range[5] = { lim::min(), T(-1), 0, T(1), lim::max() };
    for (unsigned i = 0; i < 5; ++i)
        test_conv<T>(range[i], json(range[i]), TYPE);
}

BOOST_AUTO_TEST_CASE(json_conversions) {
    test_conv<string>(string("hello"), json::str("hello"), JSON_STRING);
    BOOST_CHECK_EQUAL(to_json("world"), json::str("world"));

    test_conv_num<short>();
    test_conv_num<int>();
    test_conv_num<long>();
    test_conv_num<long long>();
    test_conv_num<unsigned short>();
    test_conv_num<unsigned>();
#if ULONG_MAX < LLONG_MAX
    test_conv_num<unsigned long>();
#endif

    test_conv_num<double, JSON_REAL>();
    test_conv_num<float,  JSON_REAL>();

    test_conv<bool>(true,  json::jtrue(),  JSON_TRUE);
    test_conv<bool>(false, json::jfalse(), JSON_FALSE);
}

BOOST_AUTO_TEST_CASE(json_create) {
    json obj1{{}};
    BOOST_CHECK(obj1);
    obj1.set("test", "set");
    BOOST_CHECK(obj1.get("test"));
    json root{
        {"obj1", obj1}
    };
    BOOST_CHECK_EQUAL(root.get("obj1"), obj1);
    obj1.set("this", "that");
    BOOST_CHECK_EQUAL(root.get("obj1").get("this").str(), "that");
    json obj2{ {"answer", 42} };
    obj1.set("obj2", obj2);
    BOOST_CHECK_EQUAL(root.get("obj1").get("obj2"), obj2);
}

#ifdef TEN_JSON_CXX11

struct corge {
    int foo, bar;

    corge() : foo(), bar() {}
    corge(int foo_, int bar_) : foo(foo_), bar(bar_) {}
};
template <class AR>
inline void serialize(AR &ar, corge &c) {
    ar & kv("foo", c.foo) & kv("bar", c.bar);
}
inline bool operator == (const corge &a, const corge &b) {
    return a.foo == b.foo && a.bar == b.bar;
}


enum captain { kirk, picard, janeway, sisko };
const array<string, 4> captain_names = {{ "kirk", "picard", "janeway", "sisko" }};
template <class AR>
inline void serialize(AR &ar, captain &c) {
    serialize_enum(ar, c, captain_names);
}


BOOST_AUTO_TEST_CASE(json_serial) {
    corge c1{42, 17};
    auto j = jsave_all(c1);
    corge c2;
    json_loader(j) >> c2;
    BOOST_CHECK(c1 == c2);

    map<string, int> m;
    json_loader(j) >> m;
    BOOST_CHECK_EQUAL(m.size(), 2);
    BOOST_CHECK(m.find("foo") != m.end());
    BOOST_CHECK(m.find("bar") != m.end());
    BOOST_CHECK_EQUAL(m["foo"], 42);
    BOOST_CHECK_EQUAL(m["bar"], 17);

#if BOOST_VERSION >= 104800
    flat_map<string, int> f;
    json_loader(j) >> f;
    BOOST_CHECK_EQUAL(f.size(), 2);
    BOOST_CHECK(f.find("foo") != f.end());
    BOOST_CHECK(f.find("bar") != f.end());
    BOOST_CHECK_EQUAL(f["foo"], 42);
    BOOST_CHECK_EQUAL(f["bar"], 17);
#endif

    maybe<int> a;
    j = jsave_all(a);
    BOOST_CHECK(!j);
    a = 42;
    j = jsave_all(a);
    BOOST_CHECK_EQUAL(j, 42);

    a.reset();
    BOOST_CHECK(!a.ok());
    j = 17;
    json_loader(j) >> a;
    BOOST_CHECK(a.ok());
    BOOST_CHECK_EQUAL(a.get(), 17);

    captain c = janeway;
    j = jsave_all(c);
    BOOST_CHECK_EQUAL(j, "janeway");
    j = "kirk";
    json_loader(j) >> c;
    BOOST_CHECK_EQUAL(c, kirk);
}

#endif // TEN_JSON_CXX11

BOOST_AUTO_TEST_CASE(json_stream) {
    // TODO: improve these tests or don't. this is a hack anyway
    using namespace jsonstream_manip;
    jsonstream s;
    s << begin_object
        << "key" << 1234
        << "list" << begin_array << "1" << 2.0f << 3.14 << 4 << 5 << end_array
    << end_object;
    BOOST_CHECK(json::load(s.str()));
}

