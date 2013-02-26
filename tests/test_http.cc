#define BOOST_TEST_MODULE http_message test
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "ten/http/http_message.hh"

using namespace ten;

// make sure static init works, this will crash if we break it
static const http_response test_static_init(200,
        http_headers("Access-Control-Allow-Origin", "*")
        );

BOOST_AUTO_TEST_CASE(http_headers_variadic_template) {
    http_request req{"GET", "/foo",
        http_headers{"This", 4, "That", "that"}
    };
    BOOST_CHECK(req.get<int>("this"));
    BOOST_CHECK_EQUAL(4, *req.get<int>("this"));
    BOOST_CHECK(req.get("that"));
    BOOST_CHECK_EQUAL("that", *req.get("that"));

    http_response resp{200, http_headers{"Thing", "stuff"}};
    BOOST_CHECK(resp.get("thing"));
    BOOST_CHECK_EQUAL("stuff", *resp.get("thing"));
}

BOOST_AUTO_TEST_CASE(http_request_constructor) {
    http_request req;
    BOOST_CHECK(req.body.empty());

    http_request req2{"POST", "/corge", {"Foo", "bar"}, "[1]", "text/json"};
    BOOST_CHECK_EQUAL("POST", req2.method);
    BOOST_CHECK_EQUAL("/corge", req2.uri);
    BOOST_CHECK_EQUAL(default_http_version, req2.version);
    BOOST_CHECK_EQUAL("text/json", *req2.get("Content-Type"));
    BOOST_CHECK_EQUAL(3, *req2.get<uint64_t>("Content-Length"));
    BOOST_CHECK_EQUAL("[1]", req2.body);
}

BOOST_AUTO_TEST_CASE(http_request_parser_init_test) {
    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(parser.data, &req);
}

BOOST_AUTO_TEST_CASE(http_request_make1) {
    http_request req{"GET", "/test/this?thing=1&stuff=2&fun&good"};
    req.append("User-Agent",
        "curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
    req.append("Host", "localhost:8080");
    req.append("Accept", "*/*");

    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(sdata, req.data());
}

BOOST_AUTO_TEST_CASE(http_request_make_parse) {
    http_request req{"GET", "/test/this?thing=1&stuff=2&fun&good"};
    req.append("user-agent",
        "curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
    req.append("host", "localhost:8080");
    req.append("accept", "*/*");
    std::string data = req.data();

    http_parser parser;
    http_request req2;
    req2.parser_init(&parser);
    BOOST_CHECK(!req2.complete);
    size_t len = data.size();
    req2.parse(&parser, data.data(), len);
    BOOST_CHECK(req2.complete);
    BOOST_CHECK_EQUAL(len, data.size());
}

BOOST_AUTO_TEST_CASE(http_request_parser_one_byte) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);

    char *data = strdupa(sdata);
    char *p = data;
    while (*p != 0 && !req.complete) {
        size_t len = 1;
        req.parse(&parser, p, len);
        p+=1; /* feed parser 1 byte at a time */
    }

    BOOST_CHECK(req.complete);
    BOOST_MESSAGE("Request:\n" << req.data());
}

BOOST_AUTO_TEST_CASE(http_request_parser_normalize_header_names) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_headers) {
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050&rn=58013009 HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n"
    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Language: en-US,en;q=0.8\r\n"
    "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\n"
    "Cookie: UID=11f039-200.050.203.060-1205406020\r\n"
    "\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_unicode_escape) {
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050&rn=58013009 HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_percents) {
    static const char *sdata =
    "GET http://bh.contextweb.com/bh/getuid?url=http://image2.pubmatic.com/AdServer/Pug?vcode=bz0yJnR5cGU9MSZqcz0xJmNvZGU9ODI1JnRsPTQzMjAw&piggybackCookie=%%CWGUID%%,User_tokens:%%USER_TOKENS%% HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_bad_percents) {
    /* this site likes to truncate urls included in their requests, which means they chop up % encoding in the original urls */
    /* for example in this url: Trends%2C%&cv= */
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?c1=8&c2=3005693&rn=698454542&c7=http%3A%2F%2Fwww.readwriteweb.com%2F&c3=1&c4=http%3A%2F%2Fwww.readwriteweb.com%2F&c8=ReadWriteWeb%20-%20Web%20Apps%2C%20Web%20Technology%20Trends%2C%&cv=2.2&cs=js HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_header_parts) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("Accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_no_headers) {
    // technically not valid because HTTP/1.1 requires a Host header
    static const char *sdata = "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
}

BOOST_AUTO_TEST_CASE(http_request_parser_garbage) {
    static const char *sdata = "\x01\xff 83949475dsf--==\x45 dsfsdfds";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    BOOST_CHECK_THROW(req.parse(&parser, sdata, len), errorx);
}

BOOST_AUTO_TEST_CASE(http_request_parser_proxy_http12) {
    static const char *sdata =
    "GET http://example.com:9182/test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("http://example.com:9182/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_clear) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    req.parser_init(&parser);
    size_t len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));

    req.clear();

    BOOST_CHECK(req.method.empty());
    BOOST_CHECK(req.uri.empty());
    BOOST_CHECK_EQUAL(req.version, default_http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);

    req.parser_init(&parser);
    len = strlen(sdata);
    req.parse(&parser, sdata, len);
    BOOST_CHECK(req.complete);

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL(http_1_1, req.version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", *req.get("Host"));
    BOOST_CHECK_EQUAL("*/*", *req.get("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_header_manipulation) {
    http_request req;
    req.append("test-a", "test-a");
    req.append("test-b", "test-b");
    req.append("test-c", "test-c");
    req.append("test-a", "test-a");

    BOOST_CHECK_EQUAL("test-a", *req.get("test-a"));
    BOOST_CHECK_EQUAL("test-b", *req.get("Test-B"));
    BOOST_CHECK_EQUAL("test-c", *req.get("TesT-c"));

    BOOST_CHECK(req.remove("Test-A"));
    BOOST_CHECK(req.remove("Test-B"));
    BOOST_CHECK(req.remove("test-c"));
    BOOST_CHECK(!req.remove("Test-A"));
}

BOOST_AUTO_TEST_CASE(http_request_host_with_underscores) {
    http_request req{"HEAD", "http://my_host_name/"};
    std::string data = req.data();

    http_parser parser;
    req.parser_init(&parser);
    size_t len = data.size();
    req.parse(&parser, data.data(), len);
    BOOST_CHECK(req.complete);
}

/* http response */

BOOST_AUTO_TEST_CASE(http_response_constructor) {
    http_response resp;
    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(default_http_version, resp.version);
    BOOST_CHECK(resp.body.empty());

    http_response resp2{200, {"Foo", "bar"}, "[1]", "text/json"};
    BOOST_CHECK_EQUAL(200, resp2.status_code);
    BOOST_CHECK_EQUAL("OK", resp2.reason());
    BOOST_CHECK_EQUAL(default_http_version, resp2.version);
    BOOST_CHECK_EQUAL("text/json", *resp2.get("Content-Type"));
    BOOST_CHECK_EQUAL(3, *resp2.get<uint64_t>("Content-Length"));
    BOOST_CHECK_EQUAL("[1]", resp2.body);
}

BOOST_AUTO_TEST_CASE(http_response_data) {
    http_response resp{200};
    resp.append("Host", "localhost");
    resp.append("Content-Length", "0");

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(http_1_1, resp.version);
    BOOST_CHECK_EQUAL("0", *resp.get("content-length"));

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(expected_data, resp.data());
}

BOOST_AUTO_TEST_CASE(http_response_body) {
    http_response resp{200};

    resp.append("Host", "localhost");

    static const char *body = "this is a test.\r\nthis is only a test.";

    resp.set_body(body, "text/plain");

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(http_1_1, resp.version);
    BOOST_CHECK_EQUAL("37", *resp.get("content-length"));
    BOOST_CHECK_EQUAL(37, *resp.get<int>("content-length"));
    BOOST_CHECK_EQUAL(body, resp.body);

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Length: 37\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(expected_data, resp.data());
}

BOOST_AUTO_TEST_CASE(http_response_parser_init_test) {
    http_response resp;
    http_parser parser;

    resp.parser_init(&parser);
    BOOST_CHECK(resp.body.empty());
    BOOST_CHECK_EQUAL(parser.data, &resp);
}

BOOST_AUTO_TEST_CASE(http_response_parser_one_byte) {
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 37\r\n"
    "Content-Type: text/plain\r\n"
    "Host: localhost\r\n\r\n"
    "this is a test.\r\nthis is only a test.";

    http_response resp;
    http_parser parser;

    resp.parser_init(&parser);
    char *data = strdupa(sdata);
    char *p = data;

    while (*p != 0 && !resp.complete) {
        size_t len = 1;
        resp.parse(&parser, p, len);
        p += 1; /* feed parser 1 byte at a time */
    }

    BOOST_CHECK(resp.complete);
    BOOST_MESSAGE("Response:\n" << resp.data() << resp.body);

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(http_1_1, resp.version);
    BOOST_CHECK_EQUAL("37", *resp.get("content-length"));
    BOOST_CHECK_EQUAL(37, *resp.get<uint64_t>("content-length"));
    BOOST_CHECK_EQUAL("text/plain", *resp.get("content-type"));
    BOOST_CHECK_EQUAL("this is a test.\r\nthis is only a test.", resp.body);
    BOOST_CHECK_EQUAL(37, resp.body_length);
}

BOOST_AUTO_TEST_CASE(http_response_parser_normalize_header_names) {
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "cOnTent-leNgtH: 37\r\n"
    "ConteNt-tYpE: text/plain\r\n"
    "HOST: localhost\r\n\r\n"
    "this is a test.\r\nthis is only a test.";

    http_response resp;
    http_parser parser;

    resp.parser_init(&parser);
    size_t len = strlen(sdata);
    resp.parse(&parser, sdata, len);
    BOOST_CHECK(resp.complete);

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(http_1_1, resp.version);
    BOOST_CHECK_EQUAL("37", *resp.get("content-length"));
    BOOST_CHECK_EQUAL(37, *resp.get<uint64_t>("content-length"));
    BOOST_CHECK_EQUAL("text/plain", *resp.get("content-type"));
    BOOST_CHECK_EQUAL("this is a test.\r\nthis is only a test.", resp.body);
    BOOST_CHECK_EQUAL(37, resp.body_length);
}

BOOST_AUTO_TEST_CASE(http_response_parser_chunked) {
    /* borrowed from http://en.wikipedia.org/wiki/Chunked_transfer_encoding */
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Transfer-Encoding: chunked\r\n\r\n"
    "25\r\n"
    "This is the data in the first chunk\r\n\r\n"
    "1C\r\n"
    "and this is the second one\r\n\r\n"
    "3\r\n"
    "con\r\n"
    "8\r\n"
    "sequence\r\n"
    "0\r\n"
    "\r\n";

    http_response resp;
    http_parser parser;

    resp.parser_init(&parser);
    size_t len = strlen(sdata);
    resp.parse(&parser, sdata, len);
    BOOST_CHECK(resp.complete);
    BOOST_MESSAGE("Response:\n" << resp.data() << resp.body);

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason());
    BOOST_CHECK_EQUAL(http_1_1, resp.version);
    BOOST_CHECK_EQUAL("text/plain", *resp.get("Content-Type"));
    BOOST_CHECK_EQUAL("chunked", *resp.get("Transfer-Encoding"));
    BOOST_CHECK_EQUAL(resp.body_length, 76);
}

BOOST_AUTO_TEST_CASE(bench_ascii_iequals) {
    using namespace std::chrono;
    using clock = std::chrono::high_resolution_clock;
    std::string ahdr = "Content-Length";
    std::string bhdr = "content-length";
    {
        auto start = clock::now();
        for (auto i=0; i<100000; ++i) {
            boost::iequals(ahdr, bhdr);
        }
        auto elapsed = clock::now() - start;
        BOOST_MESSAGE("boost::iequals: " << duration_cast<microseconds>(elapsed).count());
    }

    {
        auto start = clock::now();
        for (auto i=0; i<100000; ++i) {
            ascii_iequals(ahdr, bhdr);
        }
        auto elapsed = clock::now() - start;
        BOOST_MESSAGE("ascii_iequals: " << duration_cast<microseconds>(elapsed).count());
    }

}
