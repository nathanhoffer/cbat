cmake_minimum_required(VERSION 3.16)

project(cbat
    VERSION 0.1.0
    DESCRIPTION "C Batteries Included - a portable C99 standard library extension"
    LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CBAT_TOP_LEVEL ON)
else()
    set(CBAT_TOP_LEVEL OFF)
endif()

option(CBAT_BUILD_TESTS "Build the cbat unit tests" ${CBAT_TOP_LEVEL})
option(CBAT_BUILD_COOKBOOK "Build and run the cookbook recipes as tests" ${CBAT_TOP_LEVEL})
option(CBAT_WERROR "Treat compiler warnings as errors" OFF)
option(CBAT_SANITIZE "Build with AddressSanitizer + UBSan (GCC/Clang, non-Windows)" OFF)
option(CBAT_INSTALL "Generate install rules" ${CBAT_TOP_LEVEL})
option(CBAT_FUZZ "Build libFuzzer parser targets (Clang with ASan/UBSan)" OFF)

# Modules in dependency order. Each has include/cbat/<m>.h, src/<m>.c and
# tests/test_<m>.c.
set(CBAT_MODULES
    base
    test
    sort
    list
    heap
    hash
    string
    dict
    map
    set
    deque
    path
    fs
    file
    bytes
    url
    ini
    encoding
    csv
    json
    random
    math
    stats
    time
    log
    args
)

# ---- Warnings --------------------------------------------------------------

function(cbat_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(CBAT_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
            -Wshadow -Wstrict-prototypes -Wmissing-prototypes)
        if(CBAT_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
    if(WIN32)
        # fopen/strerror etc. are fine in portable C99 code.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(CBAT_SANITIZE AND NOT MSVC AND NOT WIN32)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(cbat_set_standard target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF)
endfunction()

# ---- Library ---------------------------------------------------------------

set(CBAT_SOURCES "")
set(CBAT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/cbat/cbat.h")
foreach(module IN LISTS CBAT_MODULES)
    list(APPEND CBAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}.c")
    list(APPEND CBAT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/cbat/${module}.h")
endforeach()

add_library(cbat ${CBAT_SOURCES} ${CBAT_HEADERS})
add_library(cbat::cbat ALIAS cbat)
cbat_set_standard(cbat)
cbat_set_warnings(cbat)
target_include_directories(cbat PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
set_target_properties(cbat PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    C_VISIBILITY_PRESET hidden)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(cbat PUBLIC CBAT_SHARED PRIVATE CBAT_BUILDING)
endif()

if(NOT WIN32)
    include(CheckLibraryExists)
    check_library_exists(m sqrt "" CBAT_HAVE_LIBM)
    if(CBAT_HAVE_LIBM)
        target_link_libraries(cbat PUBLIC m)
    endif()
endif()

# ---- Tests -----------------------------------------------------------------

if(CBAT_BUILD_TESTS OR CBAT_BUILD_COOKBOOK)
    enable_testing()
endif()

if(CBAT_BUILD_TESTS)
    foreach(module IN LISTS CBAT_MODULES)
        set(test_source "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${module}.c")
        if(EXISTS "${test_source}")
            add_executable(test_${module} "${test_source}")
            cbat_set_standard(test_${module})
            cbat_set_warnings(test_${module})
            target_link_libraries(test_${module} PRIVATE cbat::cbat)
            add_test(NAME ${module} COMMAND test_${module})
            set_tests_properties(${module} PROPERTIES
                ENVIRONMENT "CBAT_TEST_DATA=${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
        else()
            message(WARNING "cbat: module '${module}' has no tests/test_${module}.c")
        endif()
    endforeach()

    # The single-header amalgamation must compile on its own.
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        set(CBAT_AMALGAMATION "${CMAKE_CURRENT_BINARY_DIR}/dist/cbat.h")
        add_custom_command(
            OUTPUT "${CBAT_AMALGAMATION}"
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py"
                    --output "${CBAT_AMALGAMATION}"
            DEPENDS ${CBAT_SOURCES} ${CBAT_HEADERS} "${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py"
            COMMENT "Generating single-header dist/cbat.h"
            VERBATIM)
        add_custom_target(cbat_amalgamation DEPENDS "${CBAT_AMALGAMATION}")
        add_executable(test_amalgamation tests/amalgamation_main.c)
        add_dependencies(test_amalgamation cbat_amalgamation)
        cbat_set_standard(test_amalgamation)
        cbat_set_warnings(test_amalgamation)
        target_include_directories(test_amalgamation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/dist")
        if(CBAT_HAVE_LIBM)
            target_link_libraries(test_amalgamation PRIVATE m)
        endif()
        add_test(NAME amalgamation COMMAND test_amalgamation)
    endif()
endif()

if(CBAT_BUILD_COOKBOOK)
    file(GLOB CBAT_RECIPES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cookbook/*.c")
    foreach(recipe IN LISTS CBAT_RECIPES)
        get_filename_component(recipe_name "${recipe}" NAME_WE)
        set(recipe_target "cookbook_${recipe_name}")
        string(REPLACE "-" "_" recipe_target "${recipe_target}")
        add_executable(${recipe_target} "${recipe}")
        cbat_set_standard(${recipe_target})
        cbat_set_warnings(${recipe_target})
        target_link_libraries(${recipe_target} PRIVATE cbat::cbat)
        add_test(NAME "cookbook/${recipe_name}" COMMAND ${recipe_target}
                 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cookbook")
    endforeach()
endif()

if(CBAT_FUZZ)
    add_subdirectory(fuzz)
endif()

# ---- Install ---------------------------------------------------------------

if(CBAT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS cbat EXPORT cbatTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY include/cbat DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT cbatTargets
        NAMESPACE cbat::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cbat)
    configure_package_config_file(cmake/cbatConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/cbatConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cbat)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cbatConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cbatConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cbatConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cbat)
endif()
