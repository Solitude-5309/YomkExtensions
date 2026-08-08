# YomkRpc 扩展

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的 RPC 分布式通信扩展。

## 功能

> ⚠️ **注意**: 本扩展目前处于开发中状态 (WIP)，基础框架已搭建完成，具体 RPC 功能正在实现中。

| URL | 功能 | 说明 |
|-----|------|------|
| `/RpcService/version` | 版本查询 | 返回 YomkRpc 扩展版本信息 |

### 预留功能（后续添加）

- [ ] RPC 远程调用机制
- [ ] 服务注册与发现
- [ ] 负载均衡
- [ ] 链路追踪
- [ ] 序列化/反序列化协议

## 前置条件

- C++17 编译器
- CMake >= 3.14
- YomkServer 已安装

## 编译

```bash
# 默认安装到 YomkRpc/install/
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install

# 自定义安装目录
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install -DCMAKE_INSTALL_PREFIX=~/YomkServer/install
```

## 工程结构

```
YomkRpc/
├── include/
│   └── RpcService.h        # 服务头文件（消息包定义 + 类声明）
├── src/
│   └── RpcService.cpp      # 服务实现
├── test/
│   ├── CMakeLists.txt      # 测试程序构建
│   └── TestRpc.cpp         # 测试程序
├── cmake/
│   └── ProjectConfig.cmake.in  # CMake 导出配置模板
├── CMakeLists.txt            # CMake 构建配置
├── build.sh                  # 一键编译脚本
└── README.md
```

## 使用示例

```cpp
// 在工程中使用 YomkRpc
#include <YomkRpc/RpcService.h>

// 注册服务
YOMK_NEW_SERVICE(RpcService);

// 调用版本查询
YomkResponse resp = YOMK_REQUEST("/RpcService/version", nullptr);
if (resp.m_status == YomkResponse::eOk) {
    YomkUnPackPkg(resp.m_data, String, version);
    std::cout << "YomkRpc version: " << version->d << std::endl;
}
```

## 开发状态

- ✅ 基础框架搭建完成
- ✅ CMake 构建系统集成
- ✅ 测试程序框架
- 🚧 RPC 核心功能实现中
- 🚧 服务注册与发现机制
- 🚧 网络通信层

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
