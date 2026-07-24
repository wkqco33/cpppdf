cmake_minimum_required(VERSION 3.20)

project(cpppdf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 옵션 설정
option(CPPPDF_BUILD_EXAMPLES "Build cpppdf usage examples" ON)
option(BUILD_TESTING "Build unit tests" ON)

# 외부 필수/선택 패키지
find_package(ZLIB REQUIRED)
find_package(JPEG)

add_library(cpppdf STATIC
    src/cpppdf.cpp
    src/error.cpp
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

# Alias 설정 (FetchContent 지원)
add_library(cpppdf::cpppdf ALIAS cpppdf)

target_include_directories(cpppdf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cpppdf PRIVATE ZLIB::ZLIB)

if(JPEG_FOUND)
    target_compile_definitions(cpppdf PRIVATE CPPPDF_HAVE_JPEG)
    target_link_libraries(cpppdf PRIVATE JPEG::JPEG)
    message(STATUS "JPEG support enabled")
else()
    message(STATUS "JPEG support disabled (libjpeg not found)")
endif()

if(MSVC)
    target_compile_options(cpppdf PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpppdf PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 서브디렉터리
if(CPPPDF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Installation & Package Export 규칙
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS cpppdf
    EXPORT cpppdfTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/cpppdf
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT cpppdfTargets
    FILE cpppdfTargets.cmake
    NAMESPACE cpppdf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpppdf
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpppdfConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cpppdfConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpppdf
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cpppdfConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cpppdfConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cpppdfConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpppdf
)
