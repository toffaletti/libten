#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES 1
extern "C" {
#include "miniz.c"
}

#include "ten/zip.hh"

namespace ten {

std::string zip_reader::extract(int file_index, mz_uint flags) {
    if (file_index < 0) {
        throw errorx("file not found");
    }

    mz_uint64 comp_size, uncomp_size, alloc_size;
    const mz_uint8 *p = mz_zip_reader_get_cdh(&arc, file_index);
    if (!p) {
        throw errorx("mz_zip_reader_get_cdh");
    }

    comp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
    uncomp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);

    alloc_size = (flags & MZ_ZIP_FLAG_COMPRESSED_DATA) ? comp_size : uncomp_size;

    std::string data;
    data.resize(alloc_size);

    if (!mz_zip_reader_extract_to_mem(&arc, file_index, (void *)data.data(), (size_t)alloc_size, flags)) {
        throw errorx("mz_zip_reader_extract_to_mem");
    }

    return data;
}

std::string zip_reader::extract(const std::string &file_name, mz_uint flags) {
    int file_index = mz_zip_reader_locate_file(&arc, file_name.c_str(), NULL, flags);
    return extract(file_index, flags);
}

} // end namespace ten

