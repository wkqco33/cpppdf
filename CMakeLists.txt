cmake_minimum_required(VERSION 3.20)

project(cpppdf LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" AND NOT CMAKE_TOOLCHAIN_FILE)
  message(WARNING "vcpkg.json detected but CMAKE_TOOLCHAIN_FILE is not set. Packages might not be found.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(wcppcli EXCLUDE_FROM_ALL)

find_package(ZLIB REQUIRED)
find_package(JPEG)

add_library(cpppdf SHARED
    src/cpppdf.cpp
    src/parser/lexer.cpp
    src/parser/object_parser.cpp
    src/parser/xref.cpp
    src/document/document.cpp
    src/extractor/text.cpp
    src/extractor/image.cpp
    src/converter/normalizer.cpp
    src/converter/line_assembler.cpp
    src/converter/paragraph_splitter.cpp
    src/converter/table_detector.cpp
    src/converter/md_renderer.cpp
    src/converter/pdf2md.cpp
    src/renderer/terminal.cpp
)

target_include_directories(cpppdf
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(cpppdf PRIVATE ZLIB::ZLIB)

if(JPEG_FOUND)
    target_compile_definitions(cpppdf PRIVATE CPPPDF_HAVE_JPEG)
    target_link_libraries(cpppdf PRIVATE JPEG::JPEG)
    message(STATUS "JPEG support enabled")
else()
    message(STATUS "JPEG support disabled (libjpeg not found)")
endif()
target_compile_features(cpppdf PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(cpppdf PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpppdf PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cpppdf_cli cli/main.cpp)
target_link_libraries(cpppdf_cli PRIVATE cpppdf wcppcli)
target_include_directories(cpppdf_cli PRIVATE src)
set_target_properties(cpppdf_cli PROPERTIES OUTPUT_NAME cpppdf)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
