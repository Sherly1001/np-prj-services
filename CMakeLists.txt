cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(nps C)

find_package(libwebsockets CONFIG REQUIRED)
find_package(json-c CONFIG REQUIRED)

find_library(LWS_LIBS websockets REQUIRED)
find_library(JSONC_LIBS json-c REQUIRED)

find_path(LWS_INC libwebsockets.h REQUIRED)
find_path(JSONC_INC json-c/json.h REQUIRED)

set(SRC src)
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(LIBS ${LWS_LIBS} ${JSONC_LIBS})

include_directories(include ${LWS_INC} ${JSONC_INC})
file(GLOB_RECURSE SRC_FILES "${SRC}/*.c")

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
