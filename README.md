# Yomk Extensions

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的官方扩展库集合。

## 简介

YomkExtensions 是 YomkServer 的扩展模块仓库，提供了各种功能丰富的可复用服务组件。每个扩展都以共享库（`.so`）形式编译，支持通过 CMake `find_package()` 轻松集成到您的项目中。

## 核心特性

- **模块化设计**：每个扩展独立编译，只集成所需功能
- **热插拔架构**：基于 YomkServer 的"一切皆服务"理念
- **数据源无关**：扩展专注业务逻辑，不耦合具体数据源
- **CMake 友好**：提供完整的包导出配置，一键集成

## 目录结构

```
YomkExtensions/
├── YomkMath/          # 数学计算扩展（基础运算、高级函数等）
├── YomkLogAnalyzer/   # 日志分析扩展（日志解析、统计、告警等）
└── ...                # 更多扩展将持续添加
```

## 快速开始

### 1. 编译扩展

```bash
cd YomkExtensions/YomkMath
source build.sh -DCMAKE_PREFIX_PATH=/path/to/YomkServer/install
```

### 2. 在工程中使用

```cmake
# CMakeLists.txt
find_package(YomkMath REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE YomkMath::YomkMath YomkServer::YomkServer)
```

```cpp
// main.cpp
#include <YomkMath/MathService.h>

// 注册服务
YOMK_NEW_SERVICE(XxxService);

// 使用扩展功能
YomkResponse resp = YOMK_REQUEST("/MathService/add", 
    YomkMkPtr(YExtOp, ExtOp{"add", 10.5, 3.2}));
```

## 开发规范

所有扩展遵循统一的开发标准：

- **语言标准**：C++17
- **构建系统**：CMake >= 3.14
- **代码风格**：参见 [YomkServer SKILL 规范](./.qoder/skills/yomkserver-modular/SKILL.md)
- **目录结构**：`include/` + `src/` + `test/` + `cmake/`

## 贡献扩展

如需创建新的扩展，请参考技能文档中的[示例6：标准扩展模板](./.qoder/skills/yomkserver-modular/examples.md)：

```bash
# 使用模板快速创建
# 详见 examples.md 中的完整模板
```

## License

MIT License - 详见 [LICENSE.txt](LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkServer 文档](https://github.com/Solitude-5309/YomkServer#readme)
