cmake_minimum_required(VERSION 3.20)
project(prime_knots VERSION 0.2.0 LANGUAGES CXX)

option(PK_WITH_REGINA "Build the Regina classifier and the prime-knots program" ON)
option(PK_WITH_OPENMP "Classify diagrams on several threads" OFF)
include(CTest)

# Parsing and output: no Regina needed.
add_library(knot_core src/candidate.cpp)
target_include_directories(knot_core PUBLIC include)
target_compile_features(knot_core PUBLIC cxx_std_20)

if(PK_WITH_REGINA)
    # Use the compiler and linker flags of the installed Regina.
    find_program(REGINA_ENGINE_CONFIG regina-engine-config REQUIRED)
    foreach(kind cflags libs version)
        execute_process(COMMAND "${REGINA_ENGINE_CONFIG}" --${kind}
            RESULT_VARIABLE result OUTPUT_VARIABLE REGINA_${kind}
            ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "regina-engine-config --${kind}: ${error}")
        endif()
    endforeach()
    # Some builds print "Regina 7.4.1 (...)", so keep just the number.
    string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" REGINA_version "${REGINA_version}")
    if(REGINA_version VERSION_LESS "7.4")
        message(FATAL_ERROR "Regina >= 7.4 is required; found ${REGINA_version}")
    endif()
    separate_arguments(REGINA_cflags UNIX_COMMAND "${REGINA_cflags}")
    separate_arguments(REGINA_libs UNIX_COMMAND "${REGINA_libs}")

    add_library(knot_classify src/classify.cpp src/prove.cpp)
    target_link_libraries(knot_classify PUBLIC knot_core ${REGINA_libs})
    target_compile_options(knot_classify PUBLIC ${REGINA_cflags})

    add_executable(prime-knots src/main.cpp)
    target_link_libraries(prime-knots PRIVATE knot_classify)
    if(PK_WITH_OPENMP)
        find_package(OpenMP REQUIRED COMPONENTS CXX)
        target_link_libraries(prime-knots PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(test-parse tests/test_parse.cpp)
    target_link_libraries(test-parse PRIVATE knot_core)
    add_test(NAME parse COMMAND test-parse)
    if(PK_WITH_REGINA)
        add_executable(test-classify tests/test_classify.cpp)
        target_link_libraries(test-classify PRIVATE knot_classify)
        add_test(NAME classify COMMAND test-classify)
        add_test(NAME example COMMAND prime-knots "${CMAKE_CURRENT_SOURCE_DIR}/examples/knots.pd")
        set_tests_properties(classify example PROPERTIES TIMEOUT 120)
    endif()
endif()
