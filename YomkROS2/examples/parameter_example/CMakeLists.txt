cmake_minimum_required(VERSION 3.14)
project(parameter_example)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(YomkROS2 REQUIRED)
message(STATUS "YomkROS2 version: ${YomkROS2_VERSION}")
message(STATUS "YomkROS2 include dirs: ${YomkROS2_INCLUDE_DIRS}")
message(STATUS "YomkROS2 lib dir: ${YomkROS2_LIB_DIR}")
message(STATUS "YomkROS2 libraries: ${YomkROS2_LIBRARIES}")
include_directories(${YomkROS2_INCLUDE_DIRS})
link_directories(${YomkROS2_LIB_DIR})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

# node
add_executable(parameter_ctrl_node src/parameter_ctrl_main.cpp)
add_executable(parameter_exec_node src/parameter_exec_main.cpp)

# link
target_link_libraries(parameter_ctrl_node YomkROS2)
ament_target_dependencies(parameter_ctrl_node
  rclcpp
  rclcpp_action
  std_msgs
)
target_link_libraries(parameter_exec_node YomkROS2)
ament_target_dependencies(parameter_exec_node
  rclcpp
  rclcpp_action
  std_msgs
)

# install
install(TARGETS
  parameter_ctrl_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  parameter_exec_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)


ament_package()
