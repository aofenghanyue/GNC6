# YAML 配置文件使用指南

## 概述

GNC 框架现在支持 YAML 和 JSON 两种配置文件格式。YAML 格式提供了更好的可读性和注释支持，使配置文件更易于维护和理解，同时有json与yaml配置时，优先使用yaml。

## YAML 相比 JSON 的优势

### 1. 支持注释
```yaml
# 这是一个注释，可以解释配置项的用途
logger:
  level: "info"  # 日志级别：trace, debug, info, warn, error, critical
  file_enabled: true  # 是否启用文件输出
```

### 2. 更好的可读性
```yaml
# YAML 格式 - 更清晰的层次结构
logic:
  navigation:
    enabled: true
    update_frequency: 100.0
    filter_type: "perfect"
  guidance:
    enabled: true
    max_speed: 10.0
```

对比 JSON 格式：
```json
{
  "logic": {
    "navigation": {
      "enabled": true,
      "update_frequency": 100.0,
      "filter_type": "perfect"
    },
    "guidance": {
      "enabled": true,
      "max_speed": 10.0
    }
  }
}
```

### 3. 更简洁的语法
- 不需要引号包围键名
- 不需要大括号和方括号
- 使用缩进表示层次结构
- 支持多行字符串

## 配置文件格式支持

### 自动格式检测
配置管理器会自动检测文件格式：
- 通过文件扩展名：`.yaml`, `.yml` → YAML；`.json` → JSON
- 通过文件内容：检查首行特征

### 支持的文件扩展名
- YAML：`.yaml`, `.yml`
- JSON：`.json`

## 使用方法

### 1. 基本加载和保存

```cpp
#include "gnc/components/utility/config_manager.hpp"

auto& config_manager = ConfigManager::getInstance();

// 自动检测格式并加载
config_manager.loadConfigs("config/");

// 保存配置（保持原格式）
config_manager.saveConfigs();
```

### 2. 指定格式加载和保存

```cpp
// 强制加载 YAML 格式
config_manager.loadConfigs("config/", ConfigFileFormat::YAML);

// 保存为 YAML 格式
config_manager.saveConfigs(ConfigFileFormat::YAML);

// 保存为 JSON 格式
config_manager.saveConfigs(ConfigFileFormat::JSON);
```

### 3. 格式转换

```cpp
// 转换单个文件
config_manager.convertConfigFormat(
    "config/core.yaml", 
    "config/core.json", 
    ConfigFileFormat::JSON
);

// 转换所有配置文件
config_manager.convertAllConfigs("config/", ConfigFileFormat::YAML);
```

### 4. 读取和修改配置

```cpp
// 读取配置值
auto log_level = config_manager.getConfigValue<std::string>(
    ConfigFileType::CORE, "logger.level", "info");

auto nav_frequency = config_manager.getConfigValue<double>(
    ConfigFileType::LOGIC, "logic.navigation.update_frequency", 0.0);

// 修改配置值
config_manager.setConfigValue(
    ConfigFileType::CORE, "logger.level", "debug");

config_manager.setConfigValue(
    ConfigFileType::LOGIC, "logic.navigation.update_frequency", 150.0);
```

## 配置文件示例

### core.yaml
```yaml
# GNC 框架核心配置文件
# 支持注释，提高配置文件的可读性和维护性

# 日志系统配置
logger:
  console_enabled: true      # 是否启用控制台输出
  file_enabled: true         # 是否启用文件输出
  file_path: "logs/gnc.log"   # 日志文件路径
  level: "info"               # 日志级别

# 全局系统配置
global:
  simulation_time_step: 0.01    # 仿真时间步长 (秒)
  max_simulation_time: 1000.0   # 最大仿真时间 (秒)
  real_time_factor: 1.0         # 实时因子
```

### logic.yaml
```yaml
# GNC 逻辑组件配置文件

logic:
  # 导航系统配置
  navigation:
    enabled: true
    update_frequency: 100.0     # 更新频率 (Hz)
    filter_type: "perfect"       # 滤波器类型
  
  # 制导系统配置
  guidance:
    enabled: true
    max_speed: 10.0            # 最大速度 (m/s)
    max_acceleration: 2.0      # 最大加速度 (m/s²)
  
  # 控制系统配置
  control:
    enabled: true
    controller_type: "pid"       # 控制器类型
    
    # PID 控制器参数
    pid_gains:
      position:
        kp: 1.0                # 比例增益
        ki: 0.1                # 积分增益
        kd: 0.01               # 微分增益
```

## 迁移指南

### 从 JSON 迁移到 YAML

1. **使用转换工具**：
   ```cpp
   config_manager.convertAllConfigs("config/", ConfigFileFormat::YAML);
   ```

2. **手动添加注释**：
   转换后的 YAML 文件可以手动添加注释来提高可读性。

3. **验证配置**：
   确保转换后的配置文件能正确加载和使用。

### 保持兼容性

- 框架同时支持 JSON 和 YAML 格式
- 可以混合使用两种格式
- 现有的 JSON 配置文件无需修改即可继续使用

## 最佳实践

1. **使用注释**：为重要的配置项添加注释说明
2. **保持一致性**：在项目中统一使用一种格式
3. **版本控制**：将配置文件纳入版本控制
4. **备份配置**：在修改前备份重要配置
5. **验证配置**：修改后验证配置的正确性

## 故障排除

### 常见问题

1. **YAML 语法错误**：
   - 检查缩进是否正确（使用空格，不要使用制表符）
   - 确保冒号后有空格
   - 检查引号的使用

2. **文件格式检测失败**：
   - 确保文件扩展名正确
   - 检查文件内容格式

3. **配置加载失败**：
   - 检查文件路径是否正确
   - 验证文件权限
   - 查看错误日志

### 调试技巧

1. 使用示例程序测试配置加载
2. 启用详细日志输出
3. 使用在线 YAML 验证器检查语法
4. 比较转换前后的配置内容

## 总结

YAML 配置文件格式为 GNC 框架提供了更好的配置管理体验。通过支持注释和更清晰的语法，YAML 使配置文件更易于理解和维护。框架的向后兼容性确保了平滑的迁移过程。