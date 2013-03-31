#include "gtest/gtest.h"
#include <boost/algorithm/string/predicate.hpp>
#include "ten/http/http_message.hh"
#include "ten/logging.hh"

using namespace ten;

// make sure static init works, this will crash if we break it
static const http_response test_static_init(200,
        http_headers("Access-Control-Allow-Origin", "*")
        );

TEST(Http, HeadersVariadicTemplate) {
    http_request req{"GET", "/foo",
        http_headers{"This", 4, "That", "that"}
    };
    ASSERT_TRUE((bool)req.get<int>("this"));
    EXPECT_EQ(4, *req.get<int>("this"));
    ASSERT_TRUE((bool)req.get("that"));
    EXPECT_EQ("that", *req.get("that"));

    http_response resp{200, http_headers{"Thing", "stuff"}};
    ASSERT_TRUE((bool)resp.get("thing"));
    EXPECT_EQ("stuff", *resp.get("thing"));
}

TEST(Http, RequestConstructor) {
    http_request req;
    EXPECT_TRUE(req.body.empty());

    http_request req2{"POST", "/corge", {"Foo", "bar"}, "[1]", "text/json"};
    EXPECT_EQ("POST", req2.method);
    EXPECT_EQ("/corge", req2.uri);
    EXPECT_EQ(default_http_version, req2.version);
    EXPECT_EQ("text/json", *req2.get("Content-Type"));
    EXPECT_EQ(3, *req2.get<uint64_t>("Content-Length"));
    EXPECT_EQ("[1]", req2.body);
}

TEST(Http, RequestParserInit) {
    http_request req;
    req.body = "foo";
    http_parser parser;
    parser.init(req);
    EXPECT_FALSE((bool)parser);
    EXPECT_FALSE(parser.complete());
    EXPECT_TRUE(req.body.empty());
}

TEST(Http, RequestMake1) {
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

    EXPECT_EQ(sdata, req.data());
}

TEST(Http, RequestMakeParse) {
    http_request req{"GET", "/test/this?thing=1&stuff=2&fun&good"};
    req.append("user-agent",
        "curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
    req.append("host", "localhost:8080");
    req.append("accept", "*/*");
    std::string data = req.data();

    http_parser parser;
    http_request req2;
    EXPECT_FALSE(parser.complete());
    parser.init(req2);
    EXPECT_FALSE(parser.complete());
    size_t len = data.size();
    EXPECT_FALSE(parser(data.data(), len));
    EXPECT_TRUE(parser.complete());
    EXPECT_EQ(len, data.size());
}

TEST(Http, RequestParserOneByte) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);

    char *data = strdupa(sdata);
    char *p = data;
    while (*p != 0 && !parser) {
        size_t len = 1;
        parser(p, len);
        p+=1; /* feed parser 1 byte at a time */
    }

    EXPECT_TRUE(parser.complete());
    VLOG(1) << "Request:\n" << req.data();
}

TEST(Http, RequestParserNormalizeHeaderNames) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    parser(sdata, len);
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestParserHeaders) {
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
    parser.init(req);
    size_t len = strlen(sdata);
    parser.parse(sdata, len);
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestParserUnicodeEscape) {
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050&rn=58013009 HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    parser(sdata, len);
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestParserPercents) {
    static const char *sdata =
    "GET http://bh.contextweb.com/bh/getuid?url=http://image2.pubmatic.com/AdServer/Pug?vcode=bz0yJnR5cGU9MSZqcz0xJmNvZGU9ODI1JnRsPTQzMjAw&piggybackCookie=%%CWGUID%%,User_tokens:%%USER_TOKENS%% HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    parser(sdata, len);
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestParserBadPercents) {
    /* this site likes to truncate urls included in their requests, which means they chop up % encoding in the original urls */
    /* for example in this url: Trends%2C%&cv= */
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?c1=8&c2=3005693&rn=698454542&c7=http%3A%2F%2Fwww.readwriteweb.com%2F&c3=1&c4=http%3A%2F%2Fwww.readwriteweb.com%2F&c8=ReadWriteWeb%20-%20Web%20Apps%2C%20Web%20Technology%20Trends%2C%&cv=2.2&cs=js HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser.parse(sdata, len));
    EXPECT_TRUE((bool)parser);

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestParserHeaderParts) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("Accept"));
}

TEST(Http, RequestParserNoHeaders) {
    // technically not valid because HTTP/1.1 requires a Host header
    static const char *sdata = "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));
    EXPECT_TRUE(parser.complete());

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
}

TEST(Http, RequestParserGarbage) {
    static const char *sdata = "\x01\xff 83949475dsf--==\x45 dsfsdfds";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_THROW(parser(sdata, len), errorx);
}

TEST(Http, RequestParserProxyHttp12) {
    static const char *sdata =
    "GET http://example.com:9182/test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("http://example.com:9182/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestClear) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_parser parser;
    parser.init(req);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));

    req.clear();

    EXPECT_TRUE(req.method.empty());
    EXPECT_TRUE(req.uri.empty());
    EXPECT_EQ(req.version, default_http_version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);

    parser.init(req);
    len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));

    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/test/this?thing=1&stuff=2&fun&good", req.uri);
    EXPECT_EQ(http_1_1, req.version);
    EXPECT_TRUE(req.body.empty());
    EXPECT_EQ(0, req.body_length);
    EXPECT_EQ("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        *req.get("User-Agent"));
    EXPECT_EQ("localhost:8080", *req.get("Host"));
    EXPECT_EQ("*/*", *req.get("accept"));
}

TEST(Http, RequestHeaderManipulation) {
    http_request req;
    req.append("test-a", "test-a");
    req.append("test-b", "test-b");
    req.append("test-c", "test-c");
    req.append("test-a", "test-a");

    EXPECT_EQ("test-a", *req.get("test-a"));
    EXPECT_EQ("test-b", *req.get("Test-B"));
    EXPECT_EQ("test-c", *req.get("TesT-c"));

    EXPECT_TRUE(req.remove("Test-A"));
    EXPECT_TRUE(req.remove("Test-B"));
    EXPECT_TRUE(req.remove("test-c"));
    EXPECT_TRUE(!req.remove("Test-A"));
}

TEST(Http, RequestHostWithUnderscores) {
    http_request req{"HEAD", "http://my_host_name/"};
    std::string data = req.data();

    http_parser parser;
    parser.init(req);
    size_t len = data.size();
    EXPECT_FALSE(parser(data.data(), len));
}

/* http response */

TEST(Http, ResponseConstructor) {
    http_response resp{200};
    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(default_http_version, resp.version);
    EXPECT_TRUE(resp.body.empty());

    http_response resp2{200, {"Foo", "bar"}, "[1]", "text/json"};
    EXPECT_EQ(200, resp2.status_code);
    EXPECT_EQ("OK", resp2.reason());
    EXPECT_EQ(default_http_version, resp2.version);
    EXPECT_EQ("text/json", *resp2.get("Content-Type"));
    EXPECT_EQ(3, *resp2.get<uint64_t>("Content-Length"));
    EXPECT_EQ("[1]", resp2.body);
}

TEST(Http, ResponseData) {
    http_response resp{200};
    resp.append("Host", "localhost");
    resp.append("Content-Length", "0");

    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(http_1_1, resp.version);
    EXPECT_EQ("0", *resp.get("content-length"));

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

    EXPECT_EQ(expected_data, resp.data());
}

TEST(Http, ResponseBody) {
    http_response resp{200};

    resp.append("Host", "localhost");

    static const char *body = "this is a test.\r\nthis is only a test.";

    resp.set_body(body, "text/plain");

    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(http_1_1, resp.version);
    EXPECT_EQ("37", *resp.get("content-length"));
    EXPECT_EQ(37, *resp.get<int>("content-length"));
    EXPECT_EQ(body, resp.body);

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Length: 37\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

    EXPECT_EQ(expected_data, resp.data());
}

TEST(Http, ResponseParserInit) {
    http_response resp;
    http_parser parser;

    resp.body = "foo";
    parser.init(resp);
    EXPECT_TRUE(resp.body.empty());
}

TEST(Http, ResponseParserOneByte) {
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 37\r\n"
    "Content-Type: text/plain\r\n"
    "Host: localhost\r\n\r\n"
    "this is a test.\r\nthis is only a test.";

    http_response resp;
    http_parser parser;

    parser.init(resp);
    char *data = strdupa(sdata);
    char *p = data;

    while (*p != 0 && !parser.complete()) {
        size_t len = 1;
        parser(p, len);
        p += 1; /* feed parser 1 byte at a time */
    }

    EXPECT_TRUE((bool)parser);
    EXPECT_TRUE(parser.complete());
    VLOG(1) << "Response:\n" << resp.data() << resp.body;

    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(http_1_1, resp.version);
    EXPECT_EQ("37", *resp.get("content-length"));
    EXPECT_EQ(37, *resp.get<uint64_t>("content-length"));
    EXPECT_EQ("text/plain", *resp.get("content-type"));
    EXPECT_EQ("this is a test.\r\nthis is only a test.", resp.body);
    EXPECT_EQ(37, resp.body_length);
}

TEST(Http, ResponseParserNormalizeHeaderNames) {
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "cOnTent-leNgtH: 37\r\n"
    "ConteNt-tYpE: text/plain\r\n"
    "HOST: localhost\r\n\r\n"
    "this is a test.\r\nthis is only a test.";

    http_response resp;
    http_parser parser;

    parser.init(resp);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));

    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(http_1_1, resp.version);
    EXPECT_EQ("37", *resp.get("content-length"));
    EXPECT_EQ(37, *resp.get<uint64_t>("content-length"));
    EXPECT_EQ("text/plain", *resp.get("content-type"));
    EXPECT_EQ("this is a test.\r\nthis is only a test.", resp.body);
    EXPECT_EQ(37, resp.body_length);
}

TEST(Http, ResponseParserChunked) {
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

    parser.init(resp);
    size_t len = strlen(sdata);
    EXPECT_FALSE(parser(sdata, len));
    VLOG(1) << "Response:\n" << resp.data() << resp.body;

    EXPECT_EQ(200, resp.status_code);
    EXPECT_EQ("OK", resp.reason());
    EXPECT_EQ(http_1_1, resp.version);
    EXPECT_EQ("text/plain", *resp.get("Content-Type"));
    EXPECT_EQ("chunked", *resp.get("Transfer-Encoding"));
    EXPECT_EQ(resp.body_length, 76);
}

TEST(Http, BenchAsciiIequals) {
    using namespace std::chrono;
    using clock = std::chrono::high_resolution_clock;
    std::string ahdr = "Content-Length";
    std::string bhdr = "Content-length";
    {
        auto start = clock::now();
        for (auto i=0; i<100000; ++i) {
            boost::iequals(ahdr, bhdr);
        }
        auto elapsed = clock::now() - start;
        VLOG(1) << "boost::iequals: " << duration_cast<microseconds>(elapsed).count();
    }

    {
        auto start = clock::now();
        for (auto i=0; i<100000; ++i) {
            ascii_iequals(ahdr, bhdr);
        }
        auto elapsed = clock::now() - start;
        VLOG(1) << "ascii_iequals: " << duration_cast<microseconds>(elapsed).count();
    }

}
