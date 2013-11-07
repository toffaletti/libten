#include "gtest/gtest.h"

#include "ten/uri.hh"

using namespace ten;

/* TODO: test uris from:
 * http://code.google.com/p/google-url/source/browse/trunk/src/gurl_unittest.cc
 */

TEST(Uri, Constructor) {
    uri u;
    EXPECT_TRUE(u.scheme.empty());
    EXPECT_TRUE(u.userinfo.empty());
    EXPECT_TRUE(u.host.empty());
    EXPECT_TRUE(u.path.empty());
    EXPECT_TRUE(u.query.empty());
    EXPECT_TRUE(u.fragment.empty());
    EXPECT_EQ(0, u.port);
}

TEST(Uri, ParseLong) {
    static const char uri1[] = "http://www.google.com/images?hl=en&client=firefox-a&hs=tldrls=org.mozilla:en-US:official&q=philippine+hijacked+bus+picturesum=1&ie=UTF-8&source=univ&ei=SC-_TLbjE5H2tgO70PHODA&sa=X&oi=image_result_group&ct=titleresnum=1&ved=0CCIQsAQwAAbiw=1239bih=622";
    uri u{uri1};
}

TEST(Uri, ParseBrackets) {
    static const char uri1[] = "http://ad.doubleclick.net/adi/N5371.Google/B4882217.2;sz=160x600;pc=[TPAS_ID];click=http://googleads.g.doubleclick.net/aclk?sa=l&ai=Bepsf-z83TfuWJIKUjQS46ejyAeqc0t8B2uvnkxeiro6LRdC9wQEQARgBIPjy_wE4AFC0-b7IAmDJ9viGyKOgGaABnoHS5QOyAQ53d3cub3NuZXdzLmNvbboBCjE2MHg2MDBfYXPIAQnaAS9odHRwOi8vd3d3Lm9zbmV3cy5jb20vdXNlci9qYWNrZWVibGV1L2NvbW1lbnRzL7gCGMgCosXLE6gDAegDrwLoA5EG6APgBfUDAgAARPUDIAAAAA&num=1&sig=AGiWqty7uE4ibhWIPcOiZlX0__AQkpGEWA&client=ca-pub-6467510223857492&adurl=;ord=410711259?";
    uri u{uri1};
}

TEST(Uri, ParsePipe) {
    static const char uri1[] = "http://ads.pointroll.com/PortalServe/?pid=1048344U85520100615200820&flash=10time=3|18:36|-8redir=$CTURL$r=0.8149350655730814";
    uri u{uri1};
}

TEST(Uri, ParseTick) {
    static const char uri1[] = "http://chinadaily.allyes.com/s?user=ChinaDailyNetwork|NChina|NChinaUP600_80_4&db=chinadaily&border=0&local=yes&js=ie&`";
    uri u{uri1};
}

TEST(Uri, ParseUnicodeEscape) {
    static const char uri1[] = "http://b.scorecardresearch.com/b?C1=8&C2=6035047&C3=463.9924&C4=ad21868c&C5=173229&C6=16jfaue1ukmeoq&C7=http%3A//remotecontrol.mtv.com/2011/01/20/sammi-sweetheart-giancoloa-terrell-owens-hair/&C8=Hot%20Shots%3A%20Sammi%20%u2018Sweetheart%u2019%20Lets%20Terrell%20Owens%20Play%20With%20Her%20Hair%20%BB%20MTV%20Remote%20Control%20Blog&C9=&C10=1680x1050rn=58013009";
    uri u{uri1};
}

TEST(Uri, ParseDoublePercent) {
    static const char uri1[] = "http://bh.contextweb.com/bh/getuid?url=http://image2.pubmatic.com/AdServer/Pug?vcode=bz0yJnR5cGU9MSZqcz0xJmNvZGU9ODI1JnRsPTQzMjAw&piggybackCookie=%%CWGUID%%,User_tokens:%%USER_TOKENS%%";
    uri u{uri1};
}

TEST(Uri, ParsePercentAmp) {
    static const char uri1[] = "http://b.scorecardresearch.com/foo=1&cr=2%&c9=thing";
    uri u{uri1};
}

TEST(Uri, ParseBadencode) {
    static const char uri1[] = "http://b.scorecardresearch.com/b?c1=2&c2=6035223rn=1404429288&c7=http%3A%2F%2Fdetnews.com%2Farticle%2F20110121%2FMETRO01%2F101210376%2FDetroit-women-get-no-help-in-arrest-of-alleged-car-thief&c8=Detroit%20women%20get%20no%20help%20in%20arrest%20of%20alleged%2&cv=2.2&cs=js";
    uri u{uri1};
}

TEST(Uri, RussianDecode) {
    static const char uri1[] = "http://example.com:3010/path/search.proto?q=%D0%BF%D1%83%D1%82%D0%B8%D0%BD";
    uri u{uri1};
    u.normalize();
    // NOTE: the string below should appear as russian
    EXPECT_EQ(uri::decode(u.query), "?q=путин");
}

TEST(Uri, ParseQuery) {
    static const char uri1[] = "http://example.com:3010/path/search.proto?q=%D0%BF%D1%83%D1%82%D0%B8%D0%BD";
    uri u{uri1};
    u.normalize();
    uri::query_params params = u.query_part();
    // NOTE: the string below should appear as russian
    EXPECT_EQ(params.size(), 1u);
    EXPECT_EQ(params.find("q")->second, "путин");
}

TEST(Uri, ParseDups) {
    static const char uri1[] = "http://example.com:3030/path/search.proto?&&q=obama&q=obama";
    uri u{uri1};
}

TEST(Uri, Carrot) {
    // this used to throw a parser error because of ^ in the query string
    static const char path[] = "/path/search.proto?&&__geo=213&nocache=da&q=%D0%A0%D0%BE%D1%81%D1%81%D0%B8%D1%8F+^+stuff";
    uri tmp;
    tmp.host = "localhost";
    tmp.scheme = "http";
    tmp.path = path;
    uri u{tmp.compose()};
}

TEST(Uri, Transform) {
    /* examples from http://tools.ietf.org/html/rfc3986#section-5.4.1 */
    static const char base_uri[] = "http://a/b/c/d;p?q";

    uri b{base_uri};
    uri t;
    uri r;

    r.path = "g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g", t.compose());
    r.clear();
    t.clear();

    r.path = "./g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g", t.compose());
    r.clear();
    t.clear();

    r.path = "g/";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g/", t.compose());
    r.clear();
    t.clear();

    r.path = "/g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.host = "g";
    t.transform(b, r);
    EXPECT_EQ("http://g/", t.compose());
    r.clear();
    t.clear();

    r.query = "?y";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/d;p?y", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.query = "?y";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g?y", t.compose());
    r.clear();
    t.clear();

    r.fragment = "#s";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/d;p?q#s", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.fragment = "#s";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g#s", t.compose());
    r.clear();
    t.clear();

    r.path = "g";
    r.query = "?y";
    r.fragment = "#s";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g?y#s", t.compose());
    r.clear();
    t.clear();

    r.path = ";x";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/;x", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g;x", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x";
    r.query = "?y";
    r.fragment = "#s";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g;x?y#s", t.compose());
    r.clear();
    t.clear();

    r.path = "";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/d;p?q", t.compose());
    r.clear();
    t.clear();

    r.path = ".";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/", t.compose());
    r.clear();
    t.clear();

    r.path = "./";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/", t.compose());
    r.clear();
    t.clear();

    r.path = "..";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/", t.compose());
    r.clear();
    t.clear();

    r.path = "../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/g", t.compose());
    r.clear();
    t.clear();

    r.path = "../..";
    t.transform(b, r);
    EXPECT_EQ("http://a/", t.compose());
    r.clear();
    t.clear();

    r.path = "../../";
    t.transform(b, r);
    EXPECT_EQ("http://a/", t.compose());
    r.clear();
    t.clear();

    r.path = "../../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    /* abnormal examples */
    r.path = "../../../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "../../../../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "/./g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "/../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/g", t.compose());
    r.clear();
    t.clear();

    r.path = "g.";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g.", t.compose());
    r.clear();
    t.clear();

    r.path = ".g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/.g", t.compose());
    r.clear();
    t.clear();

    r.path = "g..";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g..", t.compose());
    r.clear();
    t.clear();

    r.path = "..g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/..g", t.compose());
    r.clear();
    t.clear();

    /* nonsensical */
    r.path = "./../g";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/g", t.compose());
    r.clear();
    t.clear();

    r.path = "./g/.";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g/", t.compose());
    r.clear();
    t.clear();

    r.path = "g/./h";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g/h", t.compose());
    r.clear();
    t.clear();

    r.path = "g/../h";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/h", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x=1/./y";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/g;x=1/y", t.compose());
    r.clear();
    t.clear();

    r.path = "g;x=1/../y";
    t.transform(b, r);
    EXPECT_EQ("http://a/b/c/y", t.compose());
    r.clear();
    t.clear();
}

TEST(Uri, Compose) {
    static const char uri1[] = "eXAMPLE://ExAmPlE.CoM/foo/../boo/%25hi%0b/.t%65st/./this?query=string#frag";
    uri u{uri1};
    EXPECT_EQ(uri1, u.compose());

    u.normalize();
    EXPECT_EQ("example://example.com/boo/%25hi%0B/.test/this?query=string#frag", u.compose());
}

TEST(Uri, NormalizeHost) {
    static const char uri1[] = "eXAMPLE://ExAmPlE.CoM/";
    uri u{uri1};
    u.normalize();

    EXPECT_EQ("example.com", u.host);
}

TEST(Uri, NormalizeOneSlash) {
    static const char uri1[] = "eXAMPLE://a";
    static const char uri2[] = "eXAMPLE://a/";

    uri u{uri1};
    u.normalize();
    EXPECT_EQ("/", u.path);
    u.clear();

    u.assign(uri2);
    u.normalize();
    EXPECT_EQ("/", u.path);
}

TEST(Uri, NormalizeAllSlashes) {
    static const char uri1[] = "eXAMPLE://a//////";
    uri u{uri1};
    u.normalize();
    EXPECT_EQ("/", u.path);
}

TEST(Uri, Normalize) {
    static const char uri1[] = "eXAMPLE://a/./b/../b/%63/%7bfoo%7d";
    uri u{uri1};
    u.normalize();
    EXPECT_EQ("/b/c/%7Bfoo%7D", u.path);
    u.clear();

    const char uri2[] = "http://host/../";
    u.assign(uri2);
    u.normalize();
    EXPECT_EQ("/", u.path);
    EXPECT_EQ("host", u.host);
    u.clear();

    static const char uri3[] = "http://host/./";
    u.assign(uri3);
    u.normalize();
    EXPECT_EQ("/", u.path);
    EXPECT_EQ("host", u.host);
}

TEST(Uri, ParseParts) {
    const char uri1[] = "http://example.com/path/to/something?query=string#frag";
    uri u{uri1};

    EXPECT_EQ("http", u.scheme);
    EXPECT_EQ("example.com", u.host);
    EXPECT_EQ("/path/to/something", u.path);
    EXPECT_EQ("?query=string", u.query);
    EXPECT_EQ("#frag", u.fragment);

    const char uri2[] = "http://jason:password@example.com:5555/path/to/";
    u.assign(uri2);
    EXPECT_EQ("http", u.scheme);
    EXPECT_EQ("jason:password", u.userinfo);
    EXPECT_EQ("example.com", u.host);
    EXPECT_EQ("/path/to/", u.path);
    EXPECT_EQ(5555, u.port);

    const char uri3[] = "http://baduri;f[303fds";
    EXPECT_THROW(u.assign(uri3), uri_error);

    const char uri4[] = "https://example.com:23999";
    u.assign(uri4);
    EXPECT_EQ("https", u.scheme);
    EXPECT_EQ("example.com", u.host);
    EXPECT_EQ(23999, u.port);
    EXPECT_TRUE(u.path.empty());
    EXPECT_TRUE(u.query.empty());
    EXPECT_TRUE(u.fragment.empty());

    const char uri5[] = "svn+ssh://jason:password@example.com:22/thing/and/stuff";
    u.assign(uri5);
    EXPECT_EQ("svn+ssh", u.scheme);
    EXPECT_EQ("jason:password", u.userinfo);
    EXPECT_EQ("example.com", u.host);
    EXPECT_EQ(22, u.port);
    EXPECT_EQ("/thing/and/stuff", u.path);
    EXPECT_TRUE(u.query.empty());
    EXPECT_TRUE(u.fragment.empty());
}

TEST(Uri, QueryParts) {
    uri u;
    u.query = uri::query_params{"this", "that ",
            "thing", 1234,
            "stuff", false}.str();
    EXPECT_EQ(u.query, "?this=that+&thing=1234&stuff=0");
}

TEST(Uri, QueryPartParse) {
    uri tmp;
    tmp.host = "localhost";
    tmp.scheme = "http";
    tmp.path = "/static?fn=stats.html";
    uri u(tmp.compose());
    uri::query_params qparams = u.query_part();
    auto expected = std::make_pair<std::string, std::string>("fn", "stats.html");
    auto actual = *qparams.begin();
    EXPECT_EQ(expected, actual);
}
