#define BOOST_TEST_MODULE uri test
#include <boost/test/unit_test.hpp>
#include <stdio.h>

#include "uri.hh"

/* TODO: test uris from:
 * http://code.google.com/p/google-url/source/browse/trunk/src/gurl_unittest.cc
 */

BOOST_AUTO_TEST_CASE(uri_constructor) {
    uri u;
    BOOST_CHECK(u.scheme.empty());
    BOOST_CHECK(u.userinfo.empty());
    BOOST_CHECK(u.host.empty());
    BOOST_CHECK(u.path.empty());
    BOOST_CHECK(u.query.empty());
    BOOST_CHECK(u.fragment.empty());
    BOOST_CHECK_EQUAL(0, u.port);
}

BOOST_AUTO_TEST_CASE(uri_parse_long) {
    static const char uri1[] = "http://www.google.com/images?hl=en&client=firefox-a&hs=tldrls=org.mozilla:en-US:official&q=philippine+hijacked+bus+picturesum=1&ie=UTF-8&source=univ&ei=SC-_TLbjE5H2tgO70PHODA&sa=X&oi=image_result_group&ct=titleresnum=1&ved=0CCIQsAQwAAbiw=1239bih=622";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_parse_brackets) {
    static const char uri1[] = "http://ad.doubleclick.net/adi/N5371.Google/B4882217.2;sz=160x600;pc=[TPAS_ID];click=http://googleads.g.doubleclick.net/aclk?sa=l&ai=Bepsf-z83TfuWJIKUjQS46ejyAeqc0t8B2uvnkxeiro6LRdC9wQEQARgBIPjy_wE4AFC0-b7IAmDJ9viGyKOgGaABnoHS5QOyAQ53d3cub3NuZXdzLmNvbboBCjE2MHg2MDBfYXPIAQnaAS9odHRwOi8vd3d3Lm9zbmV3cy5jb20vdXNlci9qYWNrZWVibGV1L2NvbW1lbnRzL7gCGMgCosXLE6gDAegDrwLoA5EG6APgBfUDAgAARPUDIAAAAA&num=1&sig=AGiWqty7uE4ibhWIPcOiZlX0__AQkpGEWA&client=ca-pub-6467510223857492&adurl=;ord=410711259?";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_parse_pipe) {
    static const char uri1[] = "http://ads.pointroll.com/PortalServe/?pid=1048344U85520100615200820&flash=10time=3|18:36|-8redir=$CTURL$r=0.8149350655730814";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_parse_unicode_escape) {
    static const char uri1[] = "http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050rn=58013009";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_parse_double_percent) {
    static const char uri1[] = "http://bh.contextweb.com/bh/getuid?url=http://image2.pubmatic.com/AdServer/Pug?vcode=bz0yJnR5cGU9MSZqcz0xJmNvZGU9ODI1JnRsPTQzMjAw&piggybackCookie=%%CWGUID%%,User_tokens:%%USER_TOKENS%%";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_parse_badencode) {
    static const char uri1[] = "http://b.scorecardresearch.com/b?c1=2&c2=6035223rn=1404429288&c7=http%3A%2F%2Fdetnews.com%2Farticle%2F20110121%2FMETRO01%2F101210376%2FDetroit-women-get-no-help-in-arrest-of-alleged-car-thief&c8=Detroit%20women%20get%20no%20help%20in%20arrest%20of%20alleged%2&cv=2.2&cs=js";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(uri_russian_decode) {
    static const char uri1[] = "http://example.com:3010/path/search.proto?q=%D0%BF%D1%83%D1%82%D0%B8%D0%BD";
    uri u(uri1);
    u.normalize();
    // NOTE: the string below should appear as russian
    BOOST_CHECK_EQUAL(uri::decode(u.query), "?q=путин");
}

BOOST_AUTO_TEST_CASE(uri_parse_query) {
    static const char uri1[] = "http://example.com:3010/path/search.proto?q=%D0%BF%D1%83%D1%82%D0%B8%D0%BD";
    uri u(uri1);
    u.normalize();
    uri::query_params params = u.parse_query();
    // NOTE: the string below should appear as russian
    BOOST_CHECK_EQUAL(params.size(), 1);
    BOOST_CHECK_EQUAL(params["q"], "путин");
}

BOOST_AUTO_TEST_CASE(uri_parse_dups) {
    static const char uri1[] = "http://example.com:3030/path/search.proto?&&q=obama&q=obama";
    uri u(uri1);
}

BOOST_AUTO_TEST_CASE(carrot_in_uri) {
    // this used to throw a parser error because of ^ in the query string
    static const char path[] = "/path/search.proto?&&__geo=213&nocache=da&q=%D0%A0%D0%BE%D1%81%D1%81%D0%B8%D1%8F+^+stuff";
    uri tmp;
    tmp.host = "localhost";
    tmp.scheme = "http";
    tmp.path = path;
    uri u(tmp.compose());
}

BOOST_AUTO_TEST_CASE(uri_transform) {
    /* examples from http://tools.ietf.org/html/rfc3986#section-5.4.1 */
    static const char base_uri[] = "http://a/b/c/d;p?q";

    uri b;
    uri t;
    uri r;

    BOOST_CHECK(b.parse(base_uri, strlen(base_uri)));
    r.path = "g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g", t.compose());
    r.clear();
    t.clear();

    r.path = "./g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g", t.compose());
    r.clear();
    t.clear();

    r.path = "g/";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g/", t.compose());
    r.clear();
    t.clear();

    r.path = "/g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.host = "g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://g/", t.compose());
    r.clear();
    t.clear();

    r.query = "?y";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/d;p?y", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.query = "?y";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g?y", t.compose());
    r.clear();
    t.clear();

    r.fragment = "#s";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/d;p?q#s", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.fragment = "#s";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g#s", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.query = "?y";
    r.fragment = "#s";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g?y#s", t.compose());
    r.clear();
    t.clear();

    r.path = ";x";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/;x", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g;x", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x";
    r.query = "?y";
    r.fragment = "#s";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g;x?y#s", t.compose());
    r.clear();
    t.clear();

    r.path = "";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/d;p?q", t.compose());
    r.clear();
    t.clear();

    r.path = ".";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/", t.compose());
    r.clear();
    t.clear();

    r.path = "./";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/", t.compose());
    r.clear();
    t.clear();

    r.path = "..";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/", t.compose());
    r.clear();
    t.clear();

    r.path = "../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/g", t.compose());
    r.clear();
    t.clear();

    r.path = "../..";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/", t.compose());
    r.clear();
    t.clear();

    r.path = "../../";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/", t.compose());
    r.clear();
    t.clear();

    r.path = "../../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    /* abnormal examples */
    r.path = "../../../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "../../../../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "/./g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "/../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "g.";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g.", t.compose());
    r.clear();
    t.clear();

    r.path = ".g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/.g", t.compose());
    r.clear();
    t.clear();

    r.path = "g..";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g..", t.compose());
    r.clear();
    t.clear();

    r.path = "..g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/..g", t.compose());
    r.clear();
    t.clear();

    /* nonsensical */
    r.path = "./../g";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/g", t.compose());
    r.clear();
    t.clear();

    r.path = "./g/.";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g/", t.compose());
    r.clear();
    t.clear();

    r.path = "g/./h";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g/h", t.compose());
    r.clear();
    t.clear();

    r.path = "g/../h";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/h", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x=1/./y";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/g;x=1/y", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x=1/../y";
    t.transform(b, r);
    BOOST_CHECK_EQUAL("http://a/b/c/y", t.compose());
    r.clear();
    t.clear();
}

BOOST_AUTO_TEST_CASE(uri_compose) {
    static const char uri1[] = "eXAMPLE://ExAmPlE.CoM/foo/../boo/%25hi%0b/.t%65st/./this?query=string#frag";
    uri u(uri1);
    BOOST_CHECK_EQUAL(uri1, u.compose());

    u.normalize();
    BOOST_CHECK_EQUAL("example://example.com/boo/%25hi%0B/.test/this?query=string#frag", u.compose());
}

BOOST_AUTO_TEST_CASE(uri_normalize_host) {
    static const char uri1[] = "eXAMPLE://ExAmPlE.CoM/";
    uri u(uri1);
    u.normalize();

    BOOST_CHECK_EQUAL("example.com", u.host);
}

BOOST_AUTO_TEST_CASE(uri_normalize_one_slash) {
    static const char uri1[] = "eXAMPLE://a";
    static const char uri2[] = "eXAMPLE://a/";

    uri u(uri1);
    u.normalize();
    BOOST_CHECK_EQUAL("/", u.path);
    u.clear();

    BOOST_CHECK(u.parse(uri2, strlen(uri2)));
    u.normalize();
    BOOST_CHECK_EQUAL("/", u.path);
}

BOOST_AUTO_TEST_CASE(uri_normalize_all_slashes) {
    static const char uri1[] = "eXAMPLE://a//////";
    uri u(uri1);
    u.normalize();
    BOOST_CHECK_EQUAL("/", u.path);
}

BOOST_AUTO_TEST_CASE(uri_normalize) {
    static const char uri1[] = "eXAMPLE://a/./b/../b/%63/%7bfoo%7d";
    uri u(uri1);
    u.normalize();
    BOOST_CHECK_EQUAL("/b/c/%7Bfoo%7D", u.path);
    u.clear();

    const char uri2[] = "http://host/../";
    BOOST_CHECK(u.parse(uri2, strlen(uri2)));
    u.normalize();
    BOOST_CHECK_EQUAL("/", u.path);
    BOOST_CHECK_EQUAL("host", u.host);
    u.clear();

    static const char uri3[] = "http://host/./";
    BOOST_CHECK(u.parse(uri3, strlen(uri3)));
    u.normalize();
    BOOST_CHECK_EQUAL("/", u.path);
    BOOST_CHECK_EQUAL("host", u.host);
}

BOOST_AUTO_TEST_CASE(uri_parse_parts) {
    const char uri1[] = "http://example.com/path/to/something?query=string#frag";
    uri u;

    BOOST_CHECK(u.parse(uri1, strlen(uri1)));
    BOOST_CHECK_EQUAL("http", u.scheme);
    BOOST_CHECK_EQUAL("example.com", u.host);
    BOOST_CHECK_EQUAL("/path/to/something", u.path);
    BOOST_CHECK_EQUAL("?query=string", u.query);
    BOOST_CHECK_EQUAL("#frag", u.fragment);

    const char uri2[] = "http://jason:password@example.com:5555/path/to/";
    BOOST_CHECK(u.parse(uri2, strlen(uri2)));
    BOOST_CHECK_EQUAL("http", u.scheme);
    BOOST_CHECK_EQUAL("jason:password", u.userinfo);
    BOOST_CHECK_EQUAL("example.com", u.host);
    BOOST_CHECK_EQUAL("/path/to/", u.path);
    BOOST_CHECK_EQUAL(5555, u.port);

    const char uri3[] = "http://baduri;f[303fds";
    const char *error_at = NULL;
    BOOST_CHECK(!u.parse(uri3, strlen(uri3), &error_at));
    BOOST_CHECK(error_at != NULL);
    BOOST_CHECK_EQUAL("[303fds", error_at);

    const char uri4[] = "https://example.com:23999";
    BOOST_CHECK(u.parse(uri4, strlen(uri4)));
    BOOST_CHECK_EQUAL("https", u.scheme);
    BOOST_CHECK_EQUAL("example.com", u.host);
    BOOST_CHECK_EQUAL(23999, u.port);
    BOOST_CHECK(u.path.empty());
    BOOST_CHECK(u.query.empty());
    BOOST_CHECK(u.fragment.empty());

    const char uri5[] = "svn+ssh://jason:password@example.com:22/thing/and/stuff";
    BOOST_CHECK(u.parse(uri5, strlen(uri5)));
    BOOST_CHECK_EQUAL("svn+ssh", u.scheme);
    BOOST_CHECK_EQUAL("jason:password", u.userinfo);
    BOOST_CHECK_EQUAL("example.com", u.host);
    BOOST_CHECK_EQUAL(22, u.port);
    BOOST_CHECK_EQUAL("/thing/and/stuff", u.path);
    BOOST_CHECK(u.query.empty());
    BOOST_CHECK(u.fragment.empty());
}

