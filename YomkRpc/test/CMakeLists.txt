cmake_minimum_required(VERSION 3.14)
project(TestYomkRpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(YomkServer REQUIRED)
find_package(YomkRpc REQUIRED)
message(STATUS "YomkServer version: ${YomkServer_VERSION}")  
message(STATUS "YomkServer include dirs: ${YomkServer_INCLUDE_DIRS}")  
message(STATUS "YomkServer lib dir: ${YomkServer_LIB_DIR}")  
message(STATUS "YomkServer libraries: ${YomkServer_LIBRARIES}") 
message(STATUS "YomkRpc version: ${YomkRpc_VERSION}")
message(STATUS "YomkRpc include dirs: ${YomkRpc_INCLUDE_DIRS}")
message(STATUS "YomkRpc lib dir: ${YomkRpc_LIB_DIR}")
message(STATUS "YomkRpc libraries: ${YomkRpc_LIBRARIES}")
include_directories(${YomkServer_INCLUDE_DIRS})
link_directories(${YomkServer_LIB_DIR})

add_executable(TestRpcService TestRpcService.cpp)
target_link_libraries(TestRpcService PRIVATE YomkRpc::YomkRpc YomkServer::YomkServer YomkRpcMsg)

add_executable(TestRpcLoan TestRpcLoan.cpp)
target_link_libraries(TestRpcLoan PRIVATE YomkRpc::YomkRpc YomkServer::YomkServer YomkRpcMsg)
