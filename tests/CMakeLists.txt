find_package(ZLIB REQUIRED)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE cpppdf)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME parser COMMAND test_parser)
set_tests_properties(parser PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_extractor test_extractor.cpp)
target_link_libraries(test_extractor PRIVATE cpppdf)
target_include_directories(test_extractor PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME extractor COMMAND test_extractor)
set_tests_properties(extractor PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_pdf2md test_pdf2md.cpp)
target_link_libraries(test_pdf2md PRIVATE cpppdf)
target_include_directories(test_pdf2md PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME pdf2md COMMAND test_pdf2md)
set_tests_properties(pdf2md PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(library_test library_test.cpp)
target_link_libraries(library_test PRIVATE cpppdf)
add_test(NAME library COMMAND library_test)
set_tests_properties(library PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
