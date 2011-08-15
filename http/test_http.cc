#define BOOST_TEST_MODULE http_message test
#include <boost/test/unit_test.hpp>

#include "http_message.hh"
#include "http_request_parser.h"
#include "http_response_parser.h"

BOOST_AUTO_TEST_CASE(http_request_constructor) {
    http_request req;
    BOOST_CHECK(req.body.empty());
}

BOOST_AUTO_TEST_CASE(http_request_parser_init_test) {
    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(parser.data, &req);
}

BOOST_AUTO_TEST_CASE(http_request_make1) {
    http_request req("GET", "/test/this?thing=1&stuff=2&fun&good");
    req.append_header("user-agent",
        "curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
    req.append_header("host", "localhost:8080");
    req.append_header("accept", "*/*");

    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(sdata, req.data());
}

BOOST_AUTO_TEST_CASE(http_request_make_parse) {
    http_request req("GET", "/test/this?thing=1&stuff=2&fun&good");
    req.append_header("user-agent",
        "curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
    req.append_header("host", "localhost:8080");
    req.append_header("accept", "*/*");
    std::string data = req.data();

    http_request req2;
    http_request_parser parser;
    req2.parser_init(&parser);
    http_request_parser_execute(&parser, data.c_str(), data.size(), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));
}

BOOST_AUTO_TEST_CASE(http_request_parser_one_byte) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);

    char *data = strdupa(sdata);
    char *p = data;
    while (!http_request_parser_is_finished(&parser) &&
        !http_request_parser_has_error(&parser) &&
        *p != 0)
    {
        p+=1; /* feed parser 1 byte at a time */
        http_request_parser_execute(&parser, data, p-data, p-data-1);
    }
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));
}


BOOST_AUTO_TEST_CASE(http_request_parser_normalize_header_names) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "usER-agEnT: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "host: localhost:8080\r\n"
    "ACCEPT: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL("/test/this", req.path);
    BOOST_CHECK_EQUAL("thing=1&stuff=2&fun&good", req.query_string);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
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
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_unicode_escape) {
    static const char *sdata =
    "GET http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050&rn=58013009 HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_percents) {
    static const char *sdata =
    "GET http://bh.contextweb.com/bh/getuid?url=http://image2.pubmatic.com/AdServer/Pug?vcode=bz0yJnR5cGU9MSZqcz0xJmNvZGU9ODI1JnRsPTQzMjAw&piggybackCookie=%%CWGUID%%,User_tokens:%%USER_TOKENS%% HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
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
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_header_parts) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL("/test/this", req.path);
    BOOST_CHECK_EQUAL("thing=1&stuff=2&fun&good", req.query_string);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("Accept"));
}

BOOST_AUTO_TEST_CASE(http_request_parser_no_headers) {
    static const char *sdata = "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(!http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL("/test/this", req.path);
    BOOST_CHECK_EQUAL("thing=1&stuff=2&fun&good", req.query_string);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
}

BOOST_AUTO_TEST_CASE(http_request_parser_garbage) {
    static const char *sdata = "\x01\xff 83949475dsf--==\x45 dsfsdfds";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(http_request_parser_has_error(&parser));
    BOOST_CHECK(!http_request_parser_is_finished(&parser));
}

BOOST_AUTO_TEST_CASE(http_request_parser_proxy_http12) {
    static const char *sdata =
    "GET http://example.com:9182/test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("http://example.com:9182/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK(req.path.empty());
    BOOST_CHECK(req.query_string.empty());
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_clear) {
    static const char *sdata =
    "GET /test/this?thing=1&stuff=2&fun&good HTTP/1.1\r\n"
    "User-Agent: curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18\r\n"
    "Host: localhost:8080\r\n"
    "Accept: */*\r\n\r\n";

    http_request req;
    http_request_parser parser;
    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL("/test/this", req.path);
    BOOST_CHECK_EQUAL("thing=1&stuff=2&fun&good", req.query_string);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));

    req.clear();

    BOOST_CHECK(req.method.empty());
    BOOST_CHECK(req.uri.empty());
    BOOST_CHECK(req.path.empty());
    BOOST_CHECK(req.query_string.empty());
    BOOST_CHECK(req.http_version.empty());
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);

    req.parser_init(&parser);
    http_request_parser_execute(&parser, sdata, strlen(sdata), 0);
    BOOST_CHECK(!http_request_parser_has_error(&parser));
    BOOST_CHECK(http_request_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/test/this?thing=1&stuff=2&fun&good", req.uri);
    BOOST_CHECK_EQUAL("/test/this", req.path);
    BOOST_CHECK_EQUAL("thing=1&stuff=2&fun&good", req.query_string);
    BOOST_CHECK_EQUAL("HTTP/1.1", req.http_version);
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK_EQUAL(0, req.body_length);
    BOOST_CHECK_EQUAL("curl/7.21.0 (i686-pc-linux-gnu) libcurl/7.21.0 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18",
        req.header_string("User-Agent"));
    BOOST_CHECK_EQUAL("localhost:8080", req.header_string("Host"));
    BOOST_CHECK_EQUAL("*/*", req.header_string("accept"));
}

BOOST_AUTO_TEST_CASE(http_request_header_manipulation) {
    http_request req;
    req.append_header("test-a", "test-a");
    req.append_header("test-b", "test-b");
    req.append_header("test-c", "test-c");
    req.append_header("test-a", "test-a");

    BOOST_CHECK_EQUAL("test-a", req.header_string("test-a"));
    BOOST_CHECK_EQUAL("test-b", req.header_string("Test-B"));
    BOOST_CHECK_EQUAL("test-c", req.header_string("TesT-c"));

    BOOST_CHECK(req.remove_header("Test-A"));
    BOOST_CHECK(req.remove_header("Test-B"));
    BOOST_CHECK(req.remove_header("test-c"));
    BOOST_CHECK(!req.remove_header("Test-A"));
}

/* http response */

BOOST_AUTO_TEST_CASE(http_response_constructor) {
    http_response resp;
    BOOST_CHECK(resp.body.empty());
    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
}

BOOST_AUTO_TEST_CASE(http_response_data) {
    http_response resp(200, "OK");
    resp.append_header("host", "localhost");
    resp.append_header("content-length", "0");

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
    BOOST_CHECK_EQUAL("0", resp.header_string("content-length"));

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(expected_data, resp.data());
}

BOOST_AUTO_TEST_CASE(http_response_body) {
    http_response resp(200, "OK");

    resp.append_header("host", "localhost");
    resp.append_header("content-type", "text/plain");

    static const char *body = "this is a test.\r\nthis is only a test.";

    resp.set_body(body);

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
    BOOST_CHECK_EQUAL("37", resp.header_string("content-length"));
    BOOST_CHECK_EQUAL(37, resp.header_ull("content-length"));
    BOOST_CHECK_EQUAL(body, resp.body);

    static const char *expected_data =
    "HTTP/1.1 200 OK\r\n"
    "Host: localhost\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 37\r\n"
    "\r\n";

    BOOST_CHECK_EQUAL(expected_data, resp.data());
}

BOOST_AUTO_TEST_CASE(http_response_parser_init_test) {
    http_response resp;
    http_response_parser parser;

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
    http_response_parser parser;

    resp.parser_init(&parser);
    char *data = strdupa(sdata);
    char *p = data;

    while (!http_response_parser_is_finished(&parser) &&
        !http_response_parser_has_error(&parser) &&
        *p != 0)
    {
        p += 1; /* feed parser 1 byte at a time */
        http_response_parser_execute(&parser, data, p-data, p-data-1);
    }

    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
    BOOST_CHECK_EQUAL("37", resp.header_string("content-length"));
    BOOST_CHECK_EQUAL(37, resp.header_ull("content-length"));
    BOOST_CHECK_EQUAL("text/plain", resp.header_string("content-type"));
    //BOOST_CHECK_EQUAL("this is a test.\r\nthis is only a test.", resp.body);
    //BOOST_CHECK_EQUAL(37, resp.body_length);
}

BOOST_AUTO_TEST_CASE(http_response_parser_normalize_header_names) {
    static const char *sdata =
    "HTTP/1.1 200 OK\r\n"
    "cOnTent-leNgtH: 37\r\n"
    "ConteNt-tYpE: text/plain\r\n"
    "HOST: localhost\r\n\r\n"
    "this is a test.\r\nthis is only a test.";

    http_response resp;
    http_response_parser parser;

    resp.parser_init(&parser);
    http_response_parser_execute(&parser, sdata, strlen(sdata), 0);

    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
    BOOST_CHECK_EQUAL("37", resp.header_string("content-length"));
    BOOST_CHECK_EQUAL(37, resp.header_ull("content-length"));
    BOOST_CHECK_EQUAL("text/plain", resp.header_string("content-type"));
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
    char *data = strdupa(sdata);

    http_response resp;
    http_response_parser parser;

    resp.parser_init(&parser);
    http_response_parser_execute(&parser, data, strlen(data), 0);

    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));

    BOOST_CHECK_EQUAL(200, resp.status_code);
    BOOST_CHECK_EQUAL("OK", resp.reason);
    BOOST_CHECK_EQUAL("HTTP/1.1", resp.http_version);
    BOOST_CHECK_EQUAL("text/plain", resp.header_string("Content-Type"));
    BOOST_CHECK_EQUAL("chunked", resp.header_string("Transfer-Encoding"));

    const char *mark = resp.body.c_str();
    /* reset the parse for the chunked part */
    http_response_parser_init(&parser);
    http_response_parser_execute(&parser, mark, strlen(mark), 0);
    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));
    BOOST_CHECK_EQUAL(37, resp.chunk_size);
    BOOST_CHECK_EQUAL(false, resp.last_chunk);

    mark = resp.body.c_str()+resp.chunk_size+2;
    http_response_parser_init(&parser);
    http_response_parser_execute(&parser, mark, strlen(mark), 0);
    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));
    BOOST_CHECK_EQUAL(28, resp.chunk_size);
    BOOST_CHECK_EQUAL(false, resp.last_chunk);

    mark = resp.body.c_str()+resp.chunk_size+2;
    http_response_parser_init(&parser);
    http_response_parser_execute(&parser, mark, strlen(mark), 0);
    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));
    BOOST_CHECK_EQUAL(3, resp.chunk_size);
    BOOST_CHECK_EQUAL(false, resp.last_chunk);

    mark = resp.body.c_str()+resp.chunk_size+2;
    http_response_parser_init(&parser);
    http_response_parser_execute(&parser, mark, strlen(mark), 0);
    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));
    BOOST_CHECK_EQUAL(8, resp.chunk_size);
    BOOST_CHECK_EQUAL(false, resp.last_chunk);

    mark = resp.body.c_str()+resp.chunk_size+2;
    http_response_parser_init(&parser);
    http_response_parser_execute(&parser, mark, strlen(mark), 0);
    BOOST_CHECK(!http_response_parser_has_error(&parser));
    BOOST_CHECK(http_response_parser_is_finished(&parser));
    BOOST_CHECK_EQUAL(0, resp.chunk_size);
    BOOST_CHECK_EQUAL(true, resp.last_chunk);
}

