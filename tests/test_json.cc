#define BOOST_TEST_MODULE json test
#include <boost/test/unit_test.hpp>
#include "json.hh"

#include <iostream>

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

BOOST_AUTO_TEST_CASE(json_test) {
    jsobj o(json);
    BOOST_REQUIRE(o);

    jsobj r1(o.path("/store/book/author"), true);
    std::cout << "r1: " << r1 << "\n";

    jsobj r2(o.path("//author"), true);
    std::cout << "r2: " << r2 << "\n";

    jsobj r3(o.path("/store/*"), true);
    std::cout << "r3: " << r3 << "\n";

    jsobj r4(o.path("/store//price"), true);
    std::cout << "r4: " << r4 << "\n";

    jsobj r5(o.path("//book[3]"), true);
    std::cout << "r5: " << r5 << "\n";

    jsobj r6(o.path("/store/book[3]/author"), true);
    std::cout << "r6: " << r6 << "\n";

}
