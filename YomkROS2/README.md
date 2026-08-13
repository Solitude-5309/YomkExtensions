# YomkROS2 扩展

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的 ROS2 通信扩展。

## 功能

基于 YomkServer 框架的 ROS2 扩展，预留 ROS2 分布式通信能力集成点。

| URL | 功能 | 说明 |
|-----|------|------|
| `/YomkRos2Service/version` | 版本查询 | 返回 YomkROS2 扩展版本信息 |

## 前置条件

- C++17 编译器
- CMake >= 3.14
- YomkServer 已安装

## 编译

```bash
# 默认安装到 YomkROS2/install/
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install

# 自定义安装目录
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install -DCMAKE_INSTALL_PREFIX=~/YomkServer/install
```

脚本编译安装主库后自动编译并运行 `TestYomkRos2`，并设置好 LD_LIBRARY_PATH/PATH 环境变量。

## 工程结构

```
YomkROS2/
├── include/
│   └── YomkRos2Service.h    # 服务头文件（消息包定义 + 类声明）
├── src/
│   └── YomkRos2Service.cpp  # 服务实现
├── test/
│   ├── CMakeLists.txt       # 测试程序构建
│   └── TestYomkRos2.cpp     # 服务接口端到端测试
├── cmake/
│   └── ProjectConfig.cmake.in  # CMake 导出配置模板
├── CMakeLists.txt              # CMake 构建配置
├── build.sh                    # 一键编译脚本
└── README.md
```

## 使用示例

```cpp
#include <YomkROS2/YomkRos2Service.h>

using namespace yomk;

// 启动服务
YOMK_INIT();
YOMK_NEW_SERVICE(YomkRos2Service);

// 查询版本
YomkResponse resp = YOMK_REQUEST("/YomkRos2Service/version", nullptr);
if (resp.m_status == YomkResponse::eOk)
{
    YomkUnPackPkg(resp.m_data, String, version);
    // version->d 为版本字符串
}
```

## 开发状态

- [x] 基础框架搭建完成
- [x] CMake 构建系统集成
- [ ] ROS2 集成（节点管理/主题通信）

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
