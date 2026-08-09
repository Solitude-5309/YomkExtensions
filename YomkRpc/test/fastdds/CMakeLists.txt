# FastDDS Hello World Test - Temporary Test
# Uses MString type from YomkRpcMsg

cmake_minimum_required(VERSION 3.20)
project(fastdds_hello_world_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

# Find requirements
if(NOT fastcdr_FOUND)
    find_package(fastcdr 2 REQUIRED)
endif()

if(NOT fastdds_FOUND)
    find_package(fastdds 3 REQUIRED)
endif()

# YomkRpcMsg is installed to ~/YomkServer/install without cmake config,
# so we need to find it manually.
set(YOMK_INSTALL_DIR "$ENV{HOME}/YomkServer/install")

add_executable(publisher publisher.cpp)
target_include_directories(publisher PRIVATE ${YOMK_INSTALL_DIR}/include)
target_link_directories(publisher PRIVATE ${YOMK_INSTALL_DIR}/lib)
target_link_libraries(publisher
  YomkRpcMsg
  fastdds
  fastcdr)
set_target_properties(publisher PROPERTIES
  BUILD_RPATH "${YOMK_INSTALL_DIR}/lib"
  LINK_FLAGS "-Wl,--disable-new-dtags")

add_executable(subscriber subscriber.cpp)
target_include_directories(subscriber PRIVATE ${YOMK_INSTALL_DIR}/include)
target_link_directories(subscriber PRIVATE ${YOMK_INSTALL_DIR}/lib)
target_link_libraries(subscriber
  YomkRpcMsg
  fastdds
  fastcdr)
set_target_properties(subscriber PROPERTIES
  BUILD_RPATH "${YOMK_INSTALL_DIR}/lib"
  LINK_FLAGS "-Wl,--disable-new-dtags")
