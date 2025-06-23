# GNC Meta-Framework

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/aofenghanyue/GNC6)
[![C++ Version](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)](https://github.com/aofenghanyue/GNC6)

一个强大、灵活且教学友好的GNC（制导、导航与控制）系统仿真平台。本框架采用现代C++设计，提供了一个高度可扩展的"元框架"，使用者可以轻松创建各类飞行器的仿真系统。

## 🚀 快速开始

```bash
# 克隆仓库
git clone https://github.com/aofenghanyue/GNC6.git
cd GNC6

# 创建构建目录
mkdir build && cd build

# 配置和构建
cmake ../src
make

# 运行示例
./gnc_sim
```

## 📋 目录

- [核心设计理念](#核心设计理念)
- [框架特性](#框架特性)
- [安装指南](#安装指南)
- [使用示例](#使用示例)
- [API文档](#api文档)
- [贡献指南](#贡献指南)
- [常见问题](#常见问题)
- [开发计划](#开发计划)
- [许可证](#许可证)

## 核心设计理念

### 1. 组件化架构 (Component-Based Architecture)
- 所有功能（物理模型、环境、传感器、GNC算法等）都被抽象为独立的、可插拔的组件
- 通过"搭积木"方式灵活组装，适应从简单抛射体到复杂多域飞行器的各类仿真需求
- 每个组件都继承自`ComponentBase`基类，遵循统一的接口规范

### 2. 数据驱动 (Data-Driven)
- 组件间通过中央状态管理器（`StateManager`）进行解耦通信
- 使用键值对形式存储和访问状态数据
- 支持类型安全的状态读写操作

### 3. 配置驱动 (Configuration-Driven)
- 仿真场景（组件配置、参数、初始条件等）通过外部配置文件定义
- 代码与配置分离，提高复用性和灵活性

### 4. 面向接口编程
- 大量使用抽象基类定义组件接口
- 支持策略模式，便于替换和扩展功能

### 5. 自动化依赖管理
- 根据组件声明的输入/输出依赖自动推导更新顺序
- 避免手动排序的繁琐和错误

## 框架特性

### 1. 组件即通用函数
- 组件被设计为通用计算单元
- 在配置阶段声明输入输出数据的名称（State Keys）
- 标准化的更新接口：读取输入→执行计算→写入输出

### 2. 状态管理器即通用总线
- `StateManager`作为唯一的、类型安全的数据总线
- 组件间通过约定的状态键名进行通信
- 支持多飞行器并行仿真

### 3. 适配器模式
- 提供通用的适配器组件用于数据转换
- 实现不同接口组件的无缝连接
- 支持数据拉偏、格式转换等功能

## 目录结构

```
src/
├── gnc/
│   ├── common/          # 通用类型和异常定义
│   ├── components/      # 具体组件实现
│   │   ├── dynamics/    # 动力学模型
│   │   ├── effectors/   # 执行机构
│   │   ├── environment/ # 环境模型
│   │   ├── logic/       # GNC算法
│   │   ├── sensors/     # 传感器模型
│   │   └── utility/     # 工具组件
│   └── core/           # 框架核心实现
└── main.cpp            # 示例程序
```

## 使用示例

```cpp
// 1. 创建状态管理器
StateManager manager;
const states::VehicleId VEHICLE_1 = 1;

// 2. 实例化组件
auto atmosphere = new SimpleAtmosphere(VEHICLE_1);
auto dynamics = new RigidBodyDynamics6DoF(VEHICLE_1);

// 3. 注册组件（顺序无关，框架会自动排序）
manager.registerComponent(atmosphere);
manager.registerComponent(dynamics);

// 4. 运行仿真
for (int i = 0; i < num_steps; ++i) {
    manager.updateAll();
}
```

## 开发计划

- [ ] 添加事件管理系统
- [ ] 集成单元测试框架
- [ ] 实现更多预置组件
- [ ] 添加可视化工具
- [ ] 支持分布式仿真

## 📦 安装指南

### 系统要求

- **编译器**: GCC 7.0+, Clang 6.0+, 或 MSVC 2017+
- **C++标准**: C++17 或更高版本
- **构建工具**: CMake 3.16 或更高版本
- **操作系统**: Windows 10+, Ubuntu 18.04+, macOS 10.14+

### Windows (MSYS2/MinGW)

```bash
# 安装依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake

# 构建项目
mkdir build && cd build
cmake ../src -G "MinGW Makefiles"
mingw32-make
```

### Linux/macOS

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake

# macOS (使用 Homebrew)
brew install cmake

# 构建项目
mkdir build && cd build
cmake ../src
make -j$(nproc)
```

## 📚 API文档

### 核心类

#### StateManager
状态管理器是框架的核心，负责组件生命周期和状态数据管理。

```cpp
class StateManager : public IStateAccess {
public:
    void registerComponent(ComponentBase* component);
    void updateAll();
    template<typename T>
    void setState(const StateId& id, const T& value);
    template<typename T>
    const T& getState(const StateId& id) const;
};
```

#### ComponentBase
所有组件的基类，定义了组件的标准接口。

```cpp
class ComponentBase {
protected:
    template<typename T>
    void declareInput(const std::string& name, const StateId& source);
    template<typename T>
    void declareOutput(const std::string& name, const T& initial_value = T{});
    virtual void updateImpl() = 0;
};
```

### 组件开发指南

创建自定义组件的步骤：

1. **继承ComponentBase**
```cpp
class MyComponent : public ComponentBase {
public:
    MyComponent(VehicleId id) : ComponentBase(id, "MyComponent") {
        // 声明输入输出
        declareInput<Vector3d>("input_data", source_state_id);
        declareOutput<double>("output_result");
    }
};
```

2. **实现updateImpl方法**
```cpp
protected:
    void updateImpl() override {
        auto input = getState<Vector3d>("input_data");
        double result = processData(input);
        setState("output_result", result);
    }
```

## 🤝 贡献指南

我们欢迎所有形式的贡献！请遵循以下步骤：

### 开发环境设置

1. Fork 本仓库
2. 创建功能分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add amazing feature'`
4. 推送分支: `git push origin feature/amazing-feature`
5. 创建 Pull Request

### 代码规范

- 使用现代C++特性（C++17+）
- 遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- 添加适当的注释和文档
- 确保所有测试通过

### 测试

```bash
# 运行单元测试
cd build
ctest --verbose

# 运行内存检查（Linux/macOS）
valgrind --leak-check=full ./gnc_sim
```

## ❓ 常见问题

### Q: 如何添加新的组件类型？
A: 继承`ComponentBase`类，在构造函数中声明输入输出状态，并实现`updateImpl()`方法。参考现有组件实现。

### Q: 组件的执行顺序如何确定？
A: 框架会根据组件声明的输入输出依赖关系自动进行拓扑排序，确保数据流的正确性。

### Q: 如何处理组件间的数据类型不匹配？
A: 使用适配器组件（如`BiasAdapter`）进行数据转换和格式适配。

### Q: 支持多线程吗？
A: 当前版本为单线程设计，多线程支持在开发计划中。

## 🔧 故障排除

### 编译错误
- 确保C++17支持已启用
- 检查CMake版本是否满足要求
- 验证所有依赖项已正确安装

### 运行时错误
- 检查组件依赖关系是否正确配置
- 确保状态键名匹配
- 查看控制台输出的详细错误信息

## 📈 性能优化

- 使用引用传递避免不必要的拷贝
- 合理设置仿真步长
- 考虑使用对象池管理组件实例

## 🔗 相关资源

- [GNC系统基础知识](https://example.com/gnc-basics)
- [C++17新特性](https://en.cppreference.com/w/cpp/17)
- [CMake教程](https://cmake.org/cmake/help/latest/guide/tutorial/)

## 📞 支持

如果您遇到问题或有建议，请：

- 查看[常见问题](#常见问题)部分
- 搜索现有的[Issues](https://github.com/aofenghanyue/GNC6/issues)
- 创建新的Issue描述您的问题

## 🏆 致谢

感谢所有贡献者的努力和支持！

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

---

<div align="center">
  <strong>⭐ 如果这个项目对您有帮助，请给我们一个星标！ ⭐</strong>
</div>
