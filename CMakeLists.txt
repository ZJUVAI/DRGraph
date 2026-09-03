cmake_minimum_required(VERSION 3.18)
project(DRGraph LANGUAGES C CXX)

include(CTest)
option(DRGRAPH_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(DRGRAPH_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

add_library(drgraph_core STATIC
    src/progress.cpp
    src/cli.cpp
    src/evaluation.cpp
    src/graph.cpp
    src/io.cpp
    src/hierarchy.cpp
    src/knn.cpp
    src/probability_graph.cpp
    src/sampling.cpp
    src/optimizer.cpp
    src/pipeline.cpp
    src/output.cpp)
target_include_directories(drgraph_core PUBLIC src)
target_compile_features(drgraph_core PUBLIC cxx_std_11)

find_package(Threads REQUIRED)
target_link_libraries(drgraph_core PRIVATE Threads::Threads)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(drgraph_core PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(drgraph_core PUBLIC DRGRAPH_HAVE_OPENMP)
endif()

if(DRGRAPH_ENABLE_TSAN OR CMAKE_CXX_FLAGS MATCHES "-fsanitize=thread")
    target_compile_definitions(drgraph_core PRIVATE DRGRAPH_THREAD_SANITIZER)
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx" DRGRAPH_COMPILER_HAS_MAVX)
if(OpenMP_CXX_FOUND AND DRGRAPH_COMPILER_HAS_MAVX)
    target_sources(drgraph_core PRIVATE src/efanna/knn_efanna.cpp)
    set_source_files_properties(src/efanna/knn_efanna.cpp PROPERTIES COMPILE_OPTIONS "-mavx")
    target_compile_definitions(drgraph_core PRIVATE DRGRAPH_HAVE_EFANNA)
else()
    message(WARNING "EFANNA is disabled; exact kNN remains available below 4096 vectors")
endif()

add_executable(drgraph src/main.cpp)
target_link_libraries(drgraph PRIVATE drgraph_core)
target_compile_features(drgraph PRIVATE cxx_std_11)
install(TARGETS drgraph RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    set(DRGRAPH_TEST_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
    file(MAKE_DIRECTORY "${DRGRAPH_TEST_FIXTURE_DIR}")
    foreach(fixture pipeline.data pipeline.graph valid.data duplicates.graph)
        execute_process(
            COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/prepare.sh" convert
                    "${PROJECT_SOURCE_DIR}/tests/fixtures/source/${fixture}"
                    "${DRGRAPH_TEST_FIXTURE_DIR}/${fixture}"
            RESULT_VARIABLE conversion_result)
        if(NOT conversion_result EQUAL 0)
            message(FATAL_ERROR "Failed to generate binary test input: ${fixture}")
        endif()
    endforeach()
    configure_file(${PROJECT_SOURCE_DIR}/tests/fixtures/source/pipeline.labels
                   ${DRGRAPH_TEST_FIXTURE_DIR}/pipeline.labels COPYONLY)
    add_executable(drgraph_tests tests/test_drgraph.cpp)
    target_link_libraries(drgraph_tests PRIVATE drgraph_core)
    target_compile_definitions(drgraph_tests PRIVATE TEST_FIXTURE_DIR="${DRGRAPH_TEST_FIXTURE_DIR}")
    add_test(NAME drgraph_tests COMMAND drgraph_tests)
    add_executable(drgraph_cli_tests tests/test_cli.cpp)
    target_compile_definitions(drgraph_cli_tests PRIVATE
        TEST_FIXTURE_DIR="${DRGRAPH_TEST_FIXTURE_DIR}"
        DRGRAPH_PROGRAM_PATH="$<TARGET_FILE:drgraph>")
    add_test(NAME drgraph_cli_tests COMMAND drgraph_cli_tests)
endif()
