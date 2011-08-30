# functions require cmake 2.6
# function to run ./configure; make; make install
# and properly handled cmake dependencies
# configure_flags are passed at the end
function(configure_make_install cmi_target cmi_subdir cmi_generated)
  exec_program(${CMAKE_COMMAND} ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${cmi_subdir} ${CMAKE_CURRENT_BINARY_DIR}/${cmi_subdir} OUTPUT_VARIABLE SILENT)
  add_custom_command(OUTPUT ${cmi_generated}
    COMMAND ./configure ${ARGN}
    COMMAND make
    COMMAND make install
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${cmi_subdir}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${cmi_subdir}
  )
  set_source_files_properties(${cmi_generated} PROPERTIES GENERATED TRUE)
  add_custom_target(${cmi_target} DEPENDS ${cmi_generated})
endfunction(configure_make_install)

