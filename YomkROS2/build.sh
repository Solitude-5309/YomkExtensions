#!/bin/bash
# 用法: source build.sh [额外的cmake参数...]
# 示例: source build.sh -DCMAKE_PREFIX_PATH=/path/to/YomkServer/install

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="YomkROS2"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_DIR="${SCRIPT_DIR}/install"
TEST_DIR="${SCRIPT_DIR}/test"
TEST_BUILD_DIR="${TEST_DIR}/build"
_ORIG_DIR="$(pwd)"

# 检测 ROS2 环境：未 source 时自动 source /opt/ros/humble
if [ -z "${AMENT_PREFIX_PATH}" ] && [ -f /opt/ros/humble/setup.bash ]; then
    echo "未检测到 ROS2 环境，自动 source /opt/ros/humble/setup.bash"
    source /opt/ros/humble/setup.bash
fi

# 解析用户传入的 cmake 参数
YOMK_SERVER_PATH=""
USER_INSTALL_PREFIX=""
for arg in "$@"; do
    if [[ "${arg}" == -DCMAKE_PREFIX_PATH=* ]]; then
        YOMK_SERVER_PATH="${arg#-DCMAKE_PREFIX_PATH=}"
    elif [[ "${arg}" == -DCMAKE_INSTALL_PREFIX=* ]]; then
        USER_INSTALL_PREFIX="${arg#-DCMAKE_INSTALL_PREFIX=}"
    fi
done

# 展开路径开头的 ~（bash 不会展开 -DXXX=~/path 中的 ~）
if [[ "${USER_INSTALL_PREFIX}" == "~"* ]]; then
    USER_INSTALL_PREFIX="${HOME}${USER_INSTALL_PREFIX#\~}"
fi
if [[ "${YOMK_SERVER_PATH}" == "~"* ]]; then
    YOMK_SERVER_PATH="${HOME}${YOMK_SERVER_PATH#\~}"
fi

# 用户指定了安装目录时，以用户指定的为准
if [ -n "${USER_INSTALL_PREFIX}" ]; then
    INSTALL_DIR="${USER_INSTALL_PREFIX}"
fi
echo "安装目录: ${INSTALL_DIR}"

# 询问是否编译 test
read -p "编译测试程序? [Y/n]: " BUILD_TEST
BUILD_TEST=${BUILD_TEST:-y}
if [[ "${BUILD_TEST}" =~ ^[Yy]$ ]]; then
    BUILD_TEST="ON"
else
    BUILD_TEST="OFF"
fi

# 编译安装主库
echo "步骤 1/2: 编译 ${PROJECT_NAME} 主库"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || return 1

cmake "${SCRIPT_DIR}" -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" "$@"
if [ $? -ne 0 ]; then
    echo "cmake 配置失败"
    cd "${_ORIG_DIR}"
    return 1
fi

cmake --build . --config Release --target install
if [ $? -ne 0 ]; then
    echo "编译失败"
    cd "${_ORIG_DIR}"
    return 1
fi

cd "${_ORIG_DIR}"

# 编译测试程序
if [ "${BUILD_TEST}" = "ON" ]; then
    echo ""
    echo "步骤 2/2: 编译测试程序"
    mkdir -p "${TEST_BUILD_DIR}"
    cd "${TEST_BUILD_DIR}" || return 1

    CMAKE_PREFIX_ARGS="-DCMAKE_PREFIX_PATH=${INSTALL_DIR}"
    if [ -n "${YOMK_SERVER_PATH}" ]; then
        CMAKE_PREFIX_ARGS="-DCMAKE_PREFIX_PATH=${INSTALL_DIR};${YOMK_SERVER_PATH}"
    fi

    cmake "${TEST_DIR}" ${CMAKE_PREFIX_ARGS}
    if [ $? -ne 0 ]; then
        echo "测试程序 cmake 配置失败"
        cd "${_ORIG_DIR}"
        return 1
    fi

    cmake --build . --config Release
    if [ $? -ne 0 ]; then
        echo "测试程序编译失败"
        cd "${_ORIG_DIR}"
        return 1
    fi

    # 设置临时环境变量：安装目录 lib + YomkServer lib
    LD_LIBRARY_PATH="${INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
    if [ -n "${YOMK_SERVER_PATH}" ]; then
        LD_LIBRARY_PATH="${YOMK_SERVER_PATH}/lib:${LD_LIBRARY_PATH}"
    fi
    export LD_LIBRARY_PATH
    export PATH="${TEST_BUILD_DIR}:${PATH}"

    # 运行测试
    echo ""
    echo "========== 运行非阻塞测试 =========="
    "TestYomkROS2NonBlocking"
    TEST_NONBLOCKING_RESULT=$?
    echo ""
    echo "========== 运行阻塞测试 =========="
    "TestYomkROS2Blocking"
    TEST_BLOCKING_RESULT=$?
    echo ""
    echo "========== 运行 API 宏测试（阻塞模式）=========="
    # API 测试遵循原生 ROS2 节点语义：run(true) 永久阻塞主线程，
    # 此处后台运行并延时发送 SIGINT 模拟用户 Ctrl+C，验证阻塞 run 正常返回并清理
    "TestYomkROS2API" &
    TEST_API_PID=$!
    sleep 3
    kill -INT ${TEST_API_PID} 2>/dev/null
    wait ${TEST_API_PID}
    TEST_API_RESULT=$?
    if [ ${TEST_NONBLOCKING_RESULT} -ne 0 ] || [ ${TEST_BLOCKING_RESULT} -ne 0 ] || [ ${TEST_API_RESULT} -ne 0 ]; then
        echo "存在测试失败：NonBlocking=${TEST_NONBLOCKING_RESULT} Blocking=${TEST_BLOCKING_RESULT} API=${TEST_API_RESULT}"
    fi
fi

# 返回原目录
cd "${_ORIG_DIR}" || { echo "Warning: cd failed"; exit 1; }
unset _ORIG_DIR

echo "======================================="
echo "所有编译已完成"
echo "======================================="
