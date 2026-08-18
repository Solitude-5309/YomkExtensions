# topic example

## 编译包

```

colcon build --packages-select topic_example \
  --cmake-args \
    "-DCMAKE_PREFIX_PATH=${HOME}/YomkServer/install" \
    "-DCMAKE_BUILD_TYPE=Release"

```

## 启动节点

```
export LD_LIBRARY_PATH=/home/fastdds/YomkServer/install/lib:$LD_LIBRARY_PATH

source install/setup.bash

ros2 launch topic_example topic_sub_launch.py

ros2 launch topic_example topic_pub_launch.py

```

## ros2 测试命令

```

ros2 topic pub /hello_topic std_msgs/msg/String 'data: "Hello, YomkROS2!"'

```
