#define BOOST_TEST_MODULE json test
#include <boost/test/unit_test.hpp>
#include "json.hh"

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
    json o(json::load(json_text));
    BOOST_REQUIRE(o.get());

    static const char a1[] = "[\"Nigel Rees\", \"Evelyn Waugh\", \"Herman Melville\", \"J. R. R. Tolkien\"]";
    json r1(o.path("/store/book/author"));
    BOOST_CHECK_EQUAL(json::load(a1), r1);

    json r2(o.path("//author"));
    BOOST_CHECK_EQUAL(json::load(a1), r2);

    // jansson hashtable uses size_t for hash
    // we think this is causing the buckets to change on 32bit vs. 64bit
#if (__SIZEOF_SIZE_T__ == 4)
    static const char a3[] = "[{\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}, {\"color\": \"red\", \"price\": 19.95}]";
#elif (__SIZEOF_SIZE_T__ == 8)
    static const char a3[] = "[{\"color\": \"red\", \"price\": 19.95}, {\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";
#endif
    json r3(o.path("/store/*"));
    json t3(json::load(a3));
    BOOST_CHECK_EQUAL(t3, r3);

#if (__SIZEOF_SIZE_T__ == 4)
    static const char a4[] = "[8.95, 12.99, 8.99, 22.99, 19.95]";
#elif (__SIZEOF_SIZE_T__ == 8)
    static const char a4[] = "[19.95, 8.95, 12.99, 8.99, 22.99]";
#endif
    json r4(o.path("/store//price"));
    BOOST_CHECK_EQUAL(json::load(a4), r4);

    static const char a5[] = "{\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}";
    json r5(o.path("//book[3]"));
    BOOST_CHECK_EQUAL(json::load(a5), r5);

    static const char a6[] = "\"J. R. R. Tolkien\"";
    json r6(o.path("/store/book[3]/author"));
    BOOST_CHECK_EQUAL(json::load(a6), r6);
    BOOST_CHECK(json::load(a6) == r6);

    static const char a7[] = "[{\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";
    json r7(o.path("/store/book[category=\"fiction\"]"));
    BOOST_CHECK_EQUAL(json::load(a7), r7);
}

BOOST_AUTO_TEST_CASE(json_test_path2) {
    json o(json::load("[{\"type\": 0}, {\"type\": 1}]"));
    BOOST_CHECK_EQUAL(json::load("[{\"type\":1}]"), o.path("/[type=1]"));
}

BOOST_AUTO_TEST_CASE(json_test_filter_key_exists) {
    json o(json::load(json_text));
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
    json r(o.path("//book[isbn]"));
    BOOST_CHECK_EQUAL(json::load(a), r);
    
    json r1(o.path("//book[doesnotexist]"));
    BOOST_REQUIRE(r1.is_array());
    BOOST_CHECK_EQUAL(0, r1.asize());
}

