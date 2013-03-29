#include "gtest/gtest.h"
#include "ten/zip.hh"

using namespace ten;

TEST(ZipTest, Test1) {
    EXPECT_THROW(zip_reader zr("", 0), errorx);
    zip_writer rw{1024};
    rw.add("test", "stuff", 5);
    size_t len = 0;
    auto p = rw.finalize(len);

    zip_reader zr{p.get(), len};
    mz_zip_archive_file_stat stat;
    for (auto i=0; i<zr.end(); ++i) {
        zr.stat(i, stat);
    }
    EXPECT_EQ("stuff", zr.extract("test"));
}
