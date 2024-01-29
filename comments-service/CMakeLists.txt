CMAKE_MINIMUM_REQUIRED(VERSION 3.28.1)

PROJECT(comments-service LANGUAGES CXX)

INCLUDE(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)

SET(CMAKE_CXX_STANDARD 20)

FIND_PACKAGE(Boost 1.82.0 REQUIRED)
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(nlohmann_json REQUIRED)
FIND_PACKAGE(cassandra-cpp-driver REQUIRED)
find_package(Doxygen REQUIRED)

SET(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
SET(SOURCE_DIR ${CMAKE_SOURCE_DIR}/source)

FILE(GLOB_RECURSE SOURCES
    ${INCLUDE_DIR}/*.hpp
    ${SOURCE_DIR}/*.cpp)

INCLUDE_DIRECTORIES(
    ${INCLUDE_DIR})

ADD_EXECUTABLE(${PROJECT_NAME} ${SOURCES})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE 
    ${Boost_LIBRARIES} 
    Threads::Threads
    nlohmann_json::nlohmann_json
    cassandra-cpp-driver::cassandra-cpp-driver)

if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    add_custom_target(
            documentation
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating documentation with Doxygen"
            VERBATIM
    )
endif()