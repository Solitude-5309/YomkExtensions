cmake_minimum_required(VERSION 3.14)
project(TestYomkROS2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(YomkServer REQUIRED)
find_package(YomkROS2 REQUIRED)
find_package(std_msgs REQUIRED)
message(STATUS "YomkServer version: ${YomkServer_VERSION}")
message(STATUS "YomkServer include dirs: ${YomkServer_INCLUDE_DIRS}")
message(STATUS "YomkServer lib dir: ${YomkServer_LIB_DIR}")
message(STATUS "YomkServer libraries: ${YomkServer_LIBRARIES}")
message(STATUS "YomkROS2 version: ${YomkROS2_VERSION}")
message(STATUS "YomkROS2 include dirs: ${YomkROS2_INCLUDE_DIRS}")
message(STATUS "YomkROS2 lib dir: ${YomkROS2_LIB_DIR}")
message(STATUS "YomkROS2 libraries: ${YomkROS2_LIBRARIES}")
include_directories(${YomkServer_INCLUDE_DIRS})
link_directories(${YomkServer_LIB_DIR})

add_executable(TestYomkROS2NonBlocking TestYomkROS2NonBlocking.cpp)
target_link_libraries(TestYomkROS2NonBlocking PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer std_msgs::std_msgs)

add_executable(TestYomkROS2Blocking TestYomkROS2Blocking.cpp)
target_link_libraries(TestYomkROS2Blocking PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer std_msgs::std_msgs)

add_executable(TestYomkROS2API TestYomkROS2API.cpp)
target_link_libraries(TestYomkROS2API PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer std_msgs::std_msgs)
