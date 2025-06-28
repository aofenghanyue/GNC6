# GNC 多文件配置管理系统使用指南

## 系统概述

GNC 多文件配置管理系统是对原有单文件配置系统的升级，将配置按组件类型分离到不同的JSON文件中，使配置管理更加清晰和模块化。

### 核心特性

- **分离式配置**: 按组件类型将配置分离到不同文件
- **统一接口**: 提供统一的配置访问接口
- **类型安全**: 支持模板化的类型安全配置获取
- **热重载**: 支持运行时重新加载配置
- **配置验证**: 内置配置有效性验证
- **默认值处理**: 自动提供合理的默认配置
- **变更通知**: 支持配置变更回调机制

## 配置文件结构

### 文件分类

配置文件按以下类型分离：

```
config/
├── core.json        # 核心系统配置（日志、全局参数）
├── dynamics.json    # 动力学组件配置
├── environment.json # 环境组件配置
├── effectors.json   # 效应器组件配置
├── logic.json       # 逻辑组件配置（导航、制导、控制）
├── sensors.json     # 传感器组件配置
└── utility.json     # 工具组件配置
```

### 配置文件内容

#### 1. core.json - 核心配置
```json
{
    "logger": {
        "console_enabled": true,
        "file_enabled": true,
        "file_path": "logs/gnc.log",
        "max_file_size": 10485760,
        "max_files": 5,
        "async_enabled": true,
        "level": "info"
    },
    "global": {
        "simulation_time_step": 0.01,
        "max_simulation_time": 1000.0,
        "real_time_factor": 1.0
    }
}
```

#### 2. dynamics.json - 动力学配置
```json
{
    "components": {
        "rigid_body_6dof": {
            "enabled": true,
            "mass": 1000.0,
            "inertia_matrix": [[100.0, 0.0, 0.0], [0.0, 100.0, 0.0], [0.0, 0.0, 100.0]],
            "initial_position": [0.0, 0.0, 0.0],
            "initial_velocity": [0.0, 0.0, 0.0],
            "gravity_enabled": true
        }
    }
}
```

#### 3. logic.json - 逻辑组件配置
```json
{
    "components": {
        "navigation": {
            "enabled": true,
            "update_frequency": 100.0,
            "filter_type": "perfect",
            "kalman_filter": {
                "process_noise": 0.01,
                "measurement_noise": 0.1
            }
        },
        "guidance": {
            "enabled": true,
            "update_frequency": 50.0,
            "max_speed": 10.0,
            "path_planning": {
                "algorithm": "a_star",
                "grid_resolution": 0.5
            }
        },
        "control": {
            "enabled": true,
            "update_frequency": 200.0,
            "pid_gains": {
                "position": {"kp": 1.0, "ki": 0.1, "kd": 0.01},
                "velocity": {"kp": 0.5, "ki": 0.05, "kd": 0.005}
            }
        }
    }
}
```

## 基本使用方法

### 1. 初始化配置管理器

```cpp
#include "gnc/components/utility/config_manager.hpp"

using namespace gnc::components::utility;

// 获取配置管理器实例
auto& config_manager = ConfigManager::getInstance();

// 加载所有配置文件
if (!config_manager.loadConfigs("config/")) {
    std::cerr << "Warning: Failed to load some configuration files" << std::endl;
}
```

### 2. 初始化日志系统

```cpp
#include "gnc/components/utility/simple_logger.hpp"

// 从多文件配置初始化日志系统
SimpleLogger::getInstance().initializeFromConfig("my_app");
```

### 3. 获取配置值

#### 获取组件配置
```cpp
// 获取导航组件配置
auto nav_config = config_manager.getComponentConfig(ConfigFileType::LOGIC, "navigation");
bool nav_enabled = nav_config.value("enabled", false);
double update_freq = nav_config.value("update_frequency", 100.0);

// 获取IMU传感器配置
auto imu_config = config_manager.getComponentConfig(ConfigFileType::SENSORS, "imu");
double gyro_noise = imu_config.value("gyro_noise_std", 0.01);
```

#### 使用模板方法获取配置值
```cpp
// 获取仿真时间步长
double time_step = config_manager.getConfigValue<double>(
    ConfigFileType::CORE, "global.simulation_time_step", 0.01);

// 获取日志级别
std::string log_level = config_manager.getConfigValue<std::string>(
    ConfigFileType::CORE, "logger.level", "info");

// 获取控制器PID增益
double kp = config_manager.getConfigValue<double>(
    ConfigFileType::LOGIC, "components.control.pid_gains.position.kp", 1.0);
```

#### 获取全局配置
```cpp
auto global_config = config_manager.getGlobalConfig();
double sim_time_step = global_config.value("simulation_time_step", 0.01);
double max_sim_time = global_config.value("max_simulation_time", 1000.0);
```

### 4. 修改配置值

```cpp
// 修改仿真时间步长
config_manager.setConfigValue(ConfigFileType::CORE, 
    "global.simulation_time_step", 0.005);

// 修改导航更新频率
config_manager.setConfigValue(ConfigFileType::LOGIC, 
    "components.navigation.update_frequency", 200.0);

// 修改PID增益
nlohmann::json new_gains = {
    {"kp", 2.0},
    {"ki", 0.2},
    {"kd", 0.02}
};
config_manager.setConfigValue(ConfigFileType::LOGIC, 
    "components.control.pid_gains.position", new_gains);
```

### 5. 保存配置

```cpp
// 保存所有配置文件
if (config_manager.saveConfigs()) {
    std::cout << "All configurations saved successfully" << std::endl;
}

// 保存特定类型的配置
if (config_manager.saveConfig(ConfigFileType::LOGIC)) {
    std::cout << "Logic configuration saved successfully" << std::endl;
}
```

### 6. 配置重载

```cpp
// 重新加载所有配置
if (config_manager.reloadConfigs()) {
    std::cout << "All configurations reloaded successfully" << std::endl;
}

// 重新加载特定类型的配置
if (config_manager.reloadConfig(ConfigFileType::SENSORS)) {
    std::cout << "Sensor configuration reloaded successfully" << std::endl;
}
```

## 高级功能

### 1. 配置变更通知

```cpp
// 注册配置变更回调
config_manager.registerConfigChangeCallback(
    ConfigFileType::LOGIC, 
    "navigation",
    [](ConfigFileType type, const std::string& section, const nlohmann::json& config) {
        std::cout << "Navigation config changed: " << config.dump(2) << std::endl;
    }
);
```

### 2. 配置验证

```cpp
// 验证所有配置
if (config_manager.validateConfigs()) {
    std::cout << "All configurations are valid" << std::endl;
} else {
    std::cerr << "Configuration validation failed" << std::endl;
}

// 验证特定配置
if (config_manager.validateConfig(ConfigFileType::CORE)) {
    std::cout << "Core configuration is valid" << std::endl;
}
```

### 3. 获取默认配置

```cpp
// 获取默认的逻辑组件配置
auto default_logic_config = ConfigManager::getDefaultConfig(ConfigFileType::LOGIC);
std::cout << "Default logic config: " << default_logic_config.dump(2) << std::endl;
```

## 组件集成示例

### 在组件中使用配置

```cpp
class MyNavigationComponent {
public:
    MyNavigationComponent() {
        // 获取配置管理器
        auto& config_manager = ConfigManager::getInstance();
        
        // 获取导航组件配置
        auto nav_config = config_manager.getComponentConfig(
            ConfigFileType::LOGIC, "navigation");
        
        // 读取配置参数
        enabled_ = nav_config.value("enabled", true);
        update_frequency_ = nav_config.value("update_frequency", 100.0);
        filter_type_ = nav_config.value("filter_type", "perfect");
        
        // 读取卡尔曼滤波器配置
        if (nav_config.contains("kalman_filter")) {
            auto kf_config = nav_config["kalman_filter"];
            process_noise_ = kf_config.value("process_noise", 0.01);
            measurement_noise_ = kf_config.value("measurement_noise", 0.1);
        }
        
        LOG_INFO("Navigation component initialized with config: enabled={}, freq={}", 
                enabled_, update_frequency_);
    }
    
private:
    bool enabled_;
    double update_frequency_;
    std::string filter_type_;
    double process_noise_;
    double measurement_noise_;
};
```

### 在主程序中使用

```cpp
int main() {
    try {
        // 初始化多文件配置管理器
        auto& config_manager = ConfigManager::getInstance();
        config_manager.loadConfigs("config/");
        
        // 初始化日志系统
        SimpleLogger::getInstance().initializeFromConfig("gnc_simulation");
        
        // 从配置创建组件
        auto dynamics_config = config_manager.getComponentConfig(
            ConfigFileType::DYNAMICS, "rigid_body_6dof");
        
        double mass = dynamics_config.value("mass", 1000.0);
        auto dynamics = std::make_unique<RigidBodyDynamics6DoF>(mass);
        
        // 获取仿真参数
        auto global_config = config_manager.getGlobalConfig();
        double time_step = global_config.value("simulation_time_step", 0.01);
        
        // 运行仿真...
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## 配置文件管理

### 配置文件类型映射

| 配置类型 | 文件名 | 包含组件 |
|---------|--------|----------|
| CORE | core.json | 日志系统、全局参数 |
| DYNAMICS | dynamics.json | 刚体动力学、质量特性 |
| ENVIRONMENT | environment.json | 大气模型、环境参数 |
| EFFECTORS | effectors.json | 气动力、推进系统 |
| LOGIC | logic.json | 导航、制导、控制 |
| SENSORS | sensors.json | IMU、GPS、激光雷达 |
| UTILITY | utility.json | 偏置适配器、数据记录 |

### 配置文件自动创建

如果配置文件不存在，系统会自动创建包含默认值的配置文件：

```cpp
// 系统会自动创建缺失的配置文件
config_manager.loadConfigs("config/");
// 如果 config/logic.json 不存在，会自动创建默认的逻辑组件配置
```

## 最佳实践

### 1. 配置组织

- **按功能分组**: 将相关的配置参数放在同一个配置文件中
- **使用有意义的名称**: 配置参数名称应该清晰表达其用途
- **提供注释**: 在配置文件中添加注释说明参数含义
- **设置合理默认值**: 确保默认配置能够正常工作

### 2. 配置访问

- **使用类型安全的方法**: 优先使用模板方法获取配置值
- **提供默认值**: 总是为配置获取提供合理的默认值
- **缓存频繁访问的配置**: 对于频繁访问的配置值，考虑在组件初始化时缓存

### 3. 配置修改

- **谨慎修改运行时配置**: 确保运行时配置修改不会导致系统不稳定
- **及时保存重要修改**: 对于重要的配置修改，及时保存到文件
- **使用配置验证**: 在修改配置后进行验证

### 4. 错误处理

- **优雅处理配置错误**: 配置加载失败时使用默认值继续运行
- **记录配置问题**: 使用日志系统记录配置相关的问题
- **提供配置诊断**: 实现配置诊断功能帮助用户发现问题

## 故障排除

### 常见问题

1. **配置文件加载失败**
   - 检查文件路径是否正确
   - 确认文件权限是否足够
   - 验证JSON格式是否正确

2. **配置值获取失败**
   - 检查配置路径是否正确
   - 确认配置文件类型是否匹配
   - 验证配置键是否存在

3. **配置保存失败**
   - 检查目录写权限
   - 确认磁盘空间是否足够
   - 验证文件是否被其他程序占用

### 调试技巧

```cpp
// 启用详细日志
SimpleLogger::getInstance().setLogLevel(LogLevel::DEBUG);

// 打印配置内容
auto config = config_manager.getConfig(ConfigFileType::LOGIC);
LOG_DEBUG("Logic config: {}", config.dump(2));

// 验证配置
if (!config_manager.validateConfig(ConfigFileType::LOGIC)) {
    LOG_ERROR("Logic configuration validation failed");
}
```

## 迁移指南

### 从单文件配置迁移

如果您之前使用的是单文件配置系统，可以按以下步骤迁移：

1. **备份原配置文件**
2. **运行多文件配置系统**: 系统会自动创建分离的配置文件
3. **手动调整配置值**: 根据需要调整各个配置文件中的参数
4. **更新代码**: 将配置获取代码更新为使用 `ConfigManager`
5. **测试验证**: 确保所有功能正常工作

### 代码更新示例

```cpp
// 旧代码（已删除）
// auto& config_manager = ConfigManager::getInstance();
// config_manager.loadConfig("config/gnc_config.json");
// auto nav_config = config_manager.getComponentConfig("navigation");

// 新代码
auto& config_manager = ConfigManager::getInstance();
config_manager.loadConfigs("config/");
auto nav_config = config_manager.getComponentConfig(
    ConfigFileType::LOGIC, "navigation");
```

通过使用多文件配置管理系统，您可以更好地组织和管理GNC框架的配置，提高系统的可维护性和可扩展性。