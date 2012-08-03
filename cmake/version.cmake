get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
execute_process(COMMAND "${CWD}/scm-version"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE SCM_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE SCM_VERSION)
if (NOT "${SCM_STATUS}" STREQUAL 0)
    message(FATAL_ERROR "Failed to get version from scm-version")
else()
    message("Current revision of ${PROJECT_SOURCE_DIR} is \"${SCM_VERSION}\"")
endif()

add_definitions(-DSCM_VERSION="${SCM_VERSION}")
