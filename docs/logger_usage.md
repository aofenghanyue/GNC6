# GNC 框架日志系统使用指南

## 概述

GNC 框架集成了基于 `spdlog` 的高性能日志系统，提供统一的日志接口、多级别日志记录、多输出目标支持等功能。

## 特性

- **多级别日志**：支持 TRACE、DEBUG、INFO、WARN、ERROR、CRITICAL 六个级别
- **多输出目标**：支持控制台和文件输出，文件支持自动轮转
- **高性能**：基于 spdlog 实现，支持异步日志记录
- **组件集成**：与 GNC 组件系统无缝集成
- **格式化支持**：支持 fmt 库风格的格式化字符串
- **线程安全**：支持多线程环境下的安全日志记录

## 快速开始

### 1. 初始化日志系统

```cpp
#include "gnc/components/utility/simple_logger.hpp"

using namespace gnc::components::utility;

int main() {
    // 配置日志输出
    LogSinkConfig config;
    config.console_enabled = true;           // 启用控制台输出
    config.file_enabled = true;              // 启用文件输出
    config.file_path = "logs/app.log";       // 日志文件路径
    config.max_file_size = 10 * 1024 * 1024; // 最大文件大小 10MB
    config.max_files = 5;                    // 最大文件数量
    config.async_enabled = true;             // 启用异步日志
    
    // 初始化日志系统
    SimpleLogger::getInstance().initialize(
        "my_application",     // 应用程序名称
        LogLevel::INFO,       // 日志级别
        config               // 配置
    );
    
    // 程序结束时关闭日志系统
    SimpleLogger::getInstance().shutdown();
    return 0;
}
```

### 2. 基本日志记录

```cpp
// 不同级别的日志
LOG_TRACE("详细的调试信息");
LOG_DEBUG("调试信息");
LOG_INFO("一般信息");
LOG_WARN("警告信息");
LOG_ERROR("错误信息");
LOG_CRITICAL("严重错误");

// 格式化日志
std::string user = "Alice";
int age = 30;
double score = 95.5;
LOG_INFO("用户信息: 姓名={}, 年龄={}, 分数={:.1f}", user, age, score);
```

### 3. 组件内日志记录

```cpp
class MyComponent : public ComponentBase {
public:
    MyComponent(VehicleId vehicle_id) 
        : ComponentBase(vehicle_id, "MyComponent") {
        LOG_COMPONENT_INFO("组件已创建");
    }
    
protected:
    void updateImpl() override {
        LOG_COMPONENT_DEBUG("组件更新开始");
        
        try {
            // 执行组件逻辑
            performCalculation();
            LOG_COMPONENT_INFO("计算完成");
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("计算失败: {}", e.what());
            throw;
        }
    }
    
private:
    void performCalculation() {
        // 组件具体实现
        LOG_COMPONENT_TRACE("执行详细计算步骤");
    }
};
```

## 日志级别

| 级别 | 用途 | 示例场景 |
|------|------|----------|
| TRACE | 最详细的调试信息 | 函数进入/退出、变量值跟踪 |
| DEBUG | 一般调试信息 | 算法中间结果、状态变化 |
| INFO | 程序运行信息 | 程序启动、主要操作完成 |
| WARN | 警告信息 | 参数超出正常范围、性能问题 |
| ERROR | 错误信息 | 操作失败、异常处理 |
| CRITICAL | 严重错误 | 系统崩溃、致命错误 |

## 日志宏定义

### 主日志器宏

```cpp
LOG_TRACE(...)     // 跟踪级别
LOG_DEBUG(...)     // 调试级别
LOG_INFO(...)      // 信息级别
LOG_WARN(...)      // 警告级别
LOG_ERROR(...)     // 错误级别
LOG_CRITICAL(...)  // 严重错误级别
```

### 组件日志器宏

在组件类内部使用，自动使用组件名称：

```cpp
LOG_COMPONENT_TRACE(...)
LOG_COMPONENT_DEBUG(...)
LOG_COMPONENT_INFO(...)
LOG_COMPONENT_WARN(...)
LOG_COMPONENT_ERROR(...)
LOG_COMPONENT_CRITICAL(...)
```

### 命名组件日志器宏

在组件外部使用，需要指定组件名称：

```cpp
LOG_COMPONENT_NAMED_INFO("ComponentName", "消息内容");
LOG_COMPONENT_NAMED_ERROR("ComponentName", "错误: {}", error_msg);
```

## 配置选项

### LogSinkConfig 结构体

```cpp
struct LogSinkConfig {
    bool console_enabled = true;           // 是否输出到控制台
    bool file_enabled = true;              // 是否输出到文件
    std::string file_path = "logs/gnc.log"; // 日志文件路径
    size_t max_file_size = 10 * 1024 * 1024; // 最大文件大小 (10MB)
    size_t max_files = 5;                  // 最大文件数量
    bool async_enabled = true;             // 是否启用异步日志
};
```

### 运行时配置

```cpp
auto& logger = SimpleLogger::getInstance();

// 动态调整日志级别
logger.setLogLevel(LogLevel::DEBUG);

// 强制刷新日志缓冲区
logger.flush();

// 获取主日志器
auto main_logger = logger.getMainLogger();

// 获取组件日志器
auto comp_logger = logger.getComponentLogger("MyComponent");
```

## 最佳实践

### 1. 日志级别使用建议

- **生产环境**：使用 INFO 或 WARN 级别
- **开发调试**：使用 DEBUG 或 TRACE 级别
- **性能测试**：使用 WARN 或 ERROR 级别

### 2. 日志内容建议

```cpp
// ✅ 好的日志实践
LOG_INFO("开始处理车辆 {} 的导航数据", vehicle_id);
LOG_DEBUG("计算结果: 位置=({:.2f}, {:.2f}), 速度={:.2f}", x, y, velocity);
LOG_ERROR("传感器 {} 数据读取失败: {}", sensor_name, error_msg);

// ❌ 避免的日志实践
LOG_INFO("开始");  // 信息不够具体
LOG_DEBUG(position.toString());  // 格式不统一
LOG_ERROR("错误");  // 没有错误详情
```

### 3. 性能考虑

```cpp
// ✅ 高效的日志记录
if (logger.getMainLogger()->should_log(spdlog::level::debug)) {
    std::string expensive_debug_info = generateDebugInfo();
    LOG_DEBUG("调试信息: {}", expensive_debug_info);
}

// ✅ 使用异步日志提高性能
LogSinkConfig config;
config.async_enabled = true;  // 启用异步日志

// ✅ 合理的文件轮转设置
config.max_file_size = 50 * 1024 * 1024;  // 50MB
config.max_files = 10;  // 保留10个文件
```

### 4. 错误处理

```cpp
try {
    performRiskyOperation();
    LOG_INFO("操作成功完成");
} catch (const std::exception& e) {
    LOG_ERROR("操作失败: {}", e.what());
    // 根据错误严重程度决定是否继续
    if (isCriticalError(e)) {
        LOG_CRITICAL("严重错误，程序即将退出");
        throw;
    }
}
```

## 示例程序

项目提供了完整的示例程序 `examples/logger_example.cpp`，演示了：

- 基本日志记录
- 组件内日志使用
- 日志级别控制
- 配置选项设置

编译并运行示例：

```bash
# 在项目根目录下
mkdir build && cd build
cmake ..
make

# 运行示例（如果添加到构建系统中）
./logger_example
```

## 故障排除

### 常见问题

1. **日志文件无法创建**
   - 检查目录权限
   - 确保父目录存在
   - 检查磁盘空间

2. **日志输出不完整**
   - 确保程序结束前调用 `shutdown()`
   - 检查日志级别设置
   - 考虑调用 `flush()` 强制输出

3. **性能问题**
   - 启用异步日志
   - 调整日志级别
   - 减少格式化操作的复杂度

### 调试技巧

```cpp
// 临时提高日志级别进行调试
auto& logger = SimpleLogger::getInstance();
auto old_level = LogLevel::INFO;  // 保存当前级别
logger.setLogLevel(LogLevel::TRACE);

// 执行需要调试的代码
debugFunction();

// 恢复原来的日志级别
logger.setLogLevel(old_level);
```

## 与 GNC 框架集成

日志系统已完全集成到 GNC 框架中：

- 在 `main.cpp` 中自动初始化
- 所有组件都可以直接使用组件日志宏
- 异常处理中自动记录错误日志
- 程序退出时自动关闭日志系统

这使得开发者可以专注于业务逻辑，而无需担心日志系统的管理。