include_directories(${CMAKE_CURRENT_BINARY_DIR})

function(modp_generate_header gen_name)
    add_executable(modp_${gen_name}_gen arraytoc.c modp_${gen_name}_gen.c)
    get_target_property(GEN_EXE modp_${gen_name}_gen LOCATION)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/modp_${gen_name}_data.h
        COMMAND ${GEN_EXE} > modp_${gen_name}_data.h
        DEPENDS modp_${gen_name}_gen
    )
endfunction(modp_generate_header)

modp_generate_header(ascii)
modp_generate_header(burl)
modp_generate_header(bjavascript)
modp_generate_header(b85)
modp_generate_header(b64)
modp_generate_header(b16)
modp_generate_header(b2)

add_library(stringencoders STATIC
modp_ascii.c
modp_b16.c
modp_b2.c
modp_b64.c
modp_b85.c
modp_bjavascript.c
modp_burl.c
modp_numtoa.c
${CMAKE_CURRENT_BINARY_DIR}/modp_ascii_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_burl_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_bjavascript_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_b85_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_b64_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_b16_data.h
${CMAKE_CURRENT_BINARY_DIR}/modp_b2_data.h
)
