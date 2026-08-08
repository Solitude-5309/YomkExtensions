cmake_minimum_required(VERSION 3.14)
project(TestYomkRpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXT_NAME YomkRpc)

find_package(YomkServer REQUIRED)
find_package(${EXT_NAME} REQUIRED)

message(STATUS "${EXT_NAME} version: ${${EXT_NAME}_VERSION}")
message(STATUS "${EXT_NAME} include dirs: ${${EXT_NAME}_INCLUDE_DIRS}")
message(STATUS "${EXT_NAME} lib dir: ${${EXT_NAME}_LIB_DIR}")
message(STATUS "${EXT_NAME} libraries: ${${EXT_NAME}_LIBRARIES}")

add_executable(${PROJECT_NAME} TestRpc.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXT_NAME}::${EXT_NAME} YomkServer::YomkServer)
