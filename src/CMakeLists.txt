cmake_minimum_required(VERSION 3.0.0)

include_directories(utils defbiclique)

add_subdirectory(utils)
add_subdirectory(mdb)

set(EXECUTABLE_NAME rune)

add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} utils mdb)
