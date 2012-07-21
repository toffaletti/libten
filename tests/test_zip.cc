#define BOOST_TEST_MODULE zip test
#include <boost/test/unit_test.hpp>
#include "ten/zip.hh"

using namespace ten;

BOOST_AUTO_TEST_CASE(zip_test) {
    try {
        zip_reader zr{"", 0};
        BOOST_REQUIRE(false);
    } catch (errorx &e) {
        BOOST_REQUIRE(true);
    }

    zip_writer rw{1024};
    rw.add("test", "stuff", 5);
    size_t len = 0;
    auto p = rw.finalize(len);

    zip_reader zr{p.get(), len};
    mz_zip_archive_file_stat stat;
    for (auto i=0; i<zr.end(); ++i) {
        zr.stat(i, stat);
    }
    BOOST_REQUIRE_EQUAL("stuff", zr.extract("test"));
}
