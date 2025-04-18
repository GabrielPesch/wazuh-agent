cmake_minimum_required(VERSION 3.22)

project(dbsync_unit_test)

add_definitions(-DTEST_INPUTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/)
link_directories(${CMAKE_BINARY_DIR}/lib)

file(GLOB INTERFACE_UNITTEST_SRC
    "*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/sqlite/*.cpp")

add_executable(dbsync_unit_test
    ${INTERFACE_UNITTEST_SRC})

configure_target(dbsync_unit_test)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(dbsync_unit_test
        dbsync
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        GTest::gmock_main
        cjson
    )
else()
    target_link_libraries(dbsync_unit_test
        dbsync
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        GTest::gmock_main
        cjson
        pthread
        dl
    )
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

add_test(NAME dbsync_unit_test
         COMMAND dbsync_unit_test)
