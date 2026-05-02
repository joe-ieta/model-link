cmake_minimum_required(VERSION 3.16)

# ---- ModelParser Tests ----
add_executable(test_modelparser
    test_modelparser.cpp
    ${CMAKE_SOURCE_DIR}/src/modelparser.cpp
)
target_include_directories(test_modelparser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_modelparser PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ModelParserTest COMMAND test_modelparser)

# ---- Download Integration Tests ----
add_executable(test_download
    test_download.cpp
    ${CMAKE_SOURCE_DIR}/src/downloadmanager.cpp
)
target_include_directories(test_download PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_download PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME DownloadTest COMMAND test_download)
