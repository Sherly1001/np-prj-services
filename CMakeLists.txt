cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(nps C)

find_path(LWS_INCLUDE libwebsockets.h)
find_package(libwebsockets CONFIG REQUIRED)

set(SRC src)
set(CFLAGS -Wall -Werror -Wextra)
set(LIBS websockets_shared)

include_directories(include ${LWS_INCLUDE})
file(GLOB_RECURSE SRC_FILES "${SRC}/*.c")

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
