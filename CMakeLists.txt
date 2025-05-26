cmake_minimum_required(VERSION 3.27.0 FATAL_ERROR)

project(
    wolf2wad
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Libraries
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# yyjson (0.11.1)
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.11.1
)
FetchContent_MakeAvailable(yyjson)

set(LIBS yyjson)
set(LIBTYPE STATIC)

# Build
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/*.c)
file(GLOB_RECURSE HEADERS ${SOURCE_DIR}/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
