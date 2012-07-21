get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
execute_process(COMMAND "${CWD}/scm-version"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE SCM_VERSION)
message("Current revision of ${PROJECT_SOURCE_DIR} is \"${SCM_VERSION}\"")

add_definitions(-DSCM_VERSION="${SCM_VERSION}")
