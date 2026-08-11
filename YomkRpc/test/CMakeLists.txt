cmake_minimum_required(VERSION 3.14)
project(TestYomkRpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXT_NAME YomkRpc)

find_package(YomkServer REQUIRED)
message(STATUS "YomkServer version: ${YomkServer_VERSION}")  
message(STATUS "YomkServer include dirs: ${YomkServer_INCLUDE_DIRS}")  
message(STATUS "YomkServer lib dir: ${YomkServer_LIB_DIR}")  
message(STATUS "YomkServer libraries: ${YomkServer_LIBRARIES}") 
include_directories(${YomkServer_INCLUDE_DIRS})
link_directories(${YomkServer_LIB_DIR})
find_package(${EXT_NAME} REQUIRED)

message(STATUS "${EXT_NAME} version: ${${EXT_NAME}_VERSION}")
message(STATUS "${EXT_NAME} include dirs: ${${EXT_NAME}_INCLUDE_DIRS}")
message(STATUS "${EXT_NAME} lib dir: ${${EXT_NAME}_LIB_DIR}")
message(STATUS "${EXT_NAME} libraries: ${${EXT_NAME}_LIBRARIES}")

add_executable(TestRpc TestRpc.cpp)
target_link_libraries(TestRpc PRIVATE ${EXT_NAME}::${EXT_NAME} YomkServer::YomkServer)

add_executable(TestFastDDSManager TestFastDDSManager.cpp)
target_link_libraries(TestFastDDSManager PRIVATE ${EXT_NAME}::${EXT_NAME} YomkServer::YomkServer)
