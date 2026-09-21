cmake_minimum_required(VERSION 3.20)

project(DynamicTrainLogisticsDomain LANGUAGES CXX)

include(CTest)

add_executable(dtl_tests
    Tests/src/main.cpp
    Tests/src/domain_smoke_test.cpp
    Tests/src/matcher_test.cpp
    Tests/src/quantity_test.cpp
    Tests/src/train_test.cpp
    Source/DynamicTrainLogistics/Private/Domain/Matcher.cpp
)

target_include_directories(dtl_tests PRIVATE
    Source/DynamicTrainLogistics/Public
    Tests/vendor
)

target_compile_features(dtl_tests PRIVATE cxx_std_17)
set_target_properties(dtl_tests PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(dtl_tests PRIVATE /W4)
else()
    target_compile_options(dtl_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-missing-field-initializers
    )
endif()

if(BUILD_TESTING)
    add_test(NAME domain-tests COMMAND dtl_tests)
endif()

set(CLANG_FORMAT_EXECUTABLE "clang-format" CACHE FILEPATH
    "Path to clang-format (version 18 is used in CI)"
)

file(GLOB_RECURSE DTL_FORMAT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/src/*.h"
)

add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXECUTABLE}" -i ${DTL_FORMAT_FILES}
    COMMENT "Formatting C++ sources"
    VERBATIM
)

add_custom_target(format-check
    COMMAND "${CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${DTL_FORMAT_FILES}
    COMMENT "Checking C++ source formatting"
    VERBATIM
)
