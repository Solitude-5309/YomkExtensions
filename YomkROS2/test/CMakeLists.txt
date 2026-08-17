cmake_minimum_required(VERSION 3.14)
project(TestYomkROS2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(YomkServer REQUIRED)
find_package(YomkROS2 REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(example_interfaces REQUIRED)
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

add_executable(TestYomkROS2Topic TestYomkROS2Topic.cpp)
target_link_libraries(TestYomkROS2Topic PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer std_msgs::std_msgs)

add_executable(TestYomkROS2Param TestYomkROS2Param.cpp)
target_link_libraries(TestYomkROS2Param PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer std_msgs::std_msgs)

add_executable(TestYomkROS2Service TestYomkROS2Service.cpp)
target_link_libraries(TestYomkROS2Service PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer
    std_srvs::std_srvs__rosidl_typesupport_cpp example_interfaces::example_interfaces__rosidl_typesupport_cpp)

add_executable(TestYomkROS2Action TestYomkROS2Action.cpp)
target_link_libraries(TestYomkROS2Action PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer
    example_interfaces::example_interfaces__rosidl_typesupport_cpp)

# 双进程集成测试（用户参考程序）：control_node 客户端角色 / exec_node 服务端角色
add_executable(TestYomkROS2Control TestYomkROS2Control.cpp)
target_link_libraries(TestYomkROS2Control PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer
    std_msgs::std_msgs std_srvs::std_srvs__rosidl_typesupport_cpp
    example_interfaces::example_interfaces__rosidl_typesupport_cpp)

add_executable(TestYomkROS2Exec TestYomkROS2Exec.cpp)
target_link_libraries(TestYomkROS2Exec PRIVATE YomkROS2::YomkROS2 YomkServer::YomkServer
    std_msgs::std_msgs std_srvs::std_srvs__rosidl_typesupport_cpp
    example_interfaces::example_interfaces__rosidl_typesupport_cpp)
