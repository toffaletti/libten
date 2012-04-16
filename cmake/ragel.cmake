find_program(RAGEL "ragel")

function(ragel_gen in_rl)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${in_rl}.cc
    COMMAND ${RAGEL} -G2 -o ${CMAKE_CURRENT_BINARY_DIR}/${in_rl}.cc ${CMAKE_CURRENT_SOURCE_DIR}/${in_rl}.rl -I ${CMAKE_CURRENT_SOURCE_DIR} ${ARGN}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${in_rl}.rl
    )
  add_custom_target(ragel_${in_rl} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${in_rl}.cc)
endfunction(ragel_gen)

if(RAGEL)
    message(STATUS "ragel found at: ${RAGEL}")
else(RAGEL)
    message(FATAL_ERROR "ragel not found")
endif(RAGEL)
