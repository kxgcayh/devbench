# Run at build time, including incremental builds after a checkout or local edit.
find_program(GIT_EXECUTABLE git)
set(commit "")
set(dirty -1)
if(GIT_EXECUTABLE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE git_root
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE root_status)
    file(REAL_PATH "${SOURCE_DIR}" source_root)
    if(root_status EQUAL 0 AND git_root STREQUAL source_root)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
            WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE revision
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE revision_status)
        execute_process(COMMAND "${GIT_EXECUTABLE}" --no-optional-locks status --porcelain --untracked-files=normal --ignore-submodules=none
            WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE changes
            ERROR_QUIET RESULT_VARIABLE changes_status)
        if(revision_status EQUAL 0 AND revision MATCHES "^[0-9a-f]+$")
            set(commit "${revision}")
        endif()
        if(changes_status EQUAL 0)
            set(dirty 0)
            if(NOT changes STREQUAL "")
                set(dirty 1)
            endif()
        endif()
    endif()
endif()
get_filename_component(output_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
configure_file("${SOURCE_DIR}/src/BuildInfo.h.in" "${OUTPUT_FILE}" @ONLY)
