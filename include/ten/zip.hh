#ifndef LIBTEN_ZIP_HH
#define LIBTEN_ZIP_HH

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES 1
#define MINIZ_HEADER_FILE_ONLY 1
#include "miniz.c"

#include "ten/logging.hh"
#include "ten/error.hh"
#include <string>
#include <memory>

namespace ten {

//! read a zip archive
struct zip_reader {
    mz_zip_archive arc;

    zip_reader(const void *buf, size_t len) {
        memset(&arc, 0, sizeof(mz_zip_archive));
        if (!mz_zip_reader_init_mem(&arc, buf, len, 0)) {
            throw errorx("mz_zip_reader_init_mem");
        }
    }

    int begin() { return 0; }
    int end() { return mz_zip_reader_get_num_files(&arc); }


    bool stat(int file_index, mz_zip_archive_file_stat &file_stat) {
        return mz_zip_reader_file_stat(&arc, file_index, &file_stat);
    }
    std::string extract(int index, mz_uint flags=0);
    std::string extract(const std::string &file_name, mz_uint flags=0);

    ~zip_reader() {
        CHECK(mz_zip_reader_end(&arc));
    }
};


//! write a zip archive
struct zip_writer {
    mz_zip_archive arc;

    zip_writer(size_t reserve) {
        memset(&arc, 0, sizeof(mz_zip_archive));
        if (!mz_zip_writer_init_heap(&arc, reserve, reserve)) {
            throw errorx("mz_zip_writer_init_heap");
        }
    }

    void add(const std::string &name, const void *buf, size_t len,
            mz_uint level = 7, mz_uint flags = MZ_ZIP_FLAG_COMPRESSED_DATA)
    {
        if (!mz_zip_writer_add_mem(&arc, name.c_str(), buf, len, level & flags)) {
            throw errorx("mz_zip_writer_add_mem");
        }
    }

    std::unique_ptr<void, void (*)(void *)> finalize(size_t &len) {
        void *buf = 0;
        if (!mz_zip_writer_finalize_heap_archive(&arc, &buf, &len)) {
            throw errorx("mz_zip_writer_finalize_heap_archive");
        }
        return std::unique_ptr<void, void (*)(void *)>(buf, free);
    }

    ~zip_writer() {
        CHECK(mz_zip_writer_end(&arc));
    }

};

} // end namespace ten

#endif // LIBTEN_ZIP_HH
