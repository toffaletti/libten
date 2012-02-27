#define BOOST_TEST_MODULE json test
#include <boost/test/unit_test.hpp>
#include "json.hh"

using namespace ten;

const char json[] = 
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
    jsobj o(json);
    BOOST_REQUIRE(o.ptr());

    static const char a1[] = "[\"Nigel Rees\", \"Evelyn Waugh\", \"Herman Melville\", \"J. R. R. Tolkien\"]";
    jsobj r1(o.path("/store/book/author"));
    BOOST_CHECK_EQUAL(jsobj(a1), r1);

    jsobj r2(o.path("//author"));
    BOOST_CHECK_EQUAL(jsobj(a1), r2);

    static const char a3[] = "[{\"color\": \"red\", \"price\": 19.95}, {\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";

    jsobj r3(o.path("/store/*"));
    BOOST_CHECK_EQUAL(jsobj(a3), r3);

    static const char a4[] = "[19.95, 8.95, 12.99, 8.99, 22.99]";
    jsobj r4(o.path("/store//price"));
    BOOST_CHECK_EQUAL(jsobj(a4), r4);

    static const char a5[] = "{\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}";
    jsobj r5(o.path("//book[3]"));
    BOOST_CHECK_EQUAL(jsobj(a5), r5);

    static const char a6[] = "\"J. R. R. Tolkien\"";
    jsobj r6(o.path("/store/book[3]/author"));
    BOOST_CHECK_EQUAL(jsobj(a6), r6);
    BOOST_CHECK(jsobj(a6) == r6);

    static const char a7[] = "[{\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}]";
    jsobj r7(o.path("/store/book[category=\"fiction\"]"));
    BOOST_CHECK_EQUAL(jsobj(a7), r7);
}
