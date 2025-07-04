# 坐标转换系统使用指南

本文档介绍如何在GNC仿真框架中使用坐标转换系统。

## 快速开始

### 1. 基本集成

坐标转换系统通过配置文件自动集成。只需在 `config/core.yaml` 中为您的飞行器添加 `CoordinationInitializer` 组件：

```yaml
vehicles:
  - id: 1
    components:
      # 坐标转换系统初始化器（建议放在第一个）
      - CoordinationInitializer
      # 其他组件...
      - RigidBodyDynamics6DoF
      - SimpleAerodynamics
      # ...
```

框架会自动处理组件注册和初始化。

### 2. 在组件中使用坐标转换

```cpp
#include "gnc/coordination/coordination.hpp"
#include "gnc/components/utility/coordination_initializer.hpp"

class MyGNCComponent : public states::ComponentBase {
public:
    MyGNCComponent(states::VehicleId id) : states::ComponentBase(id, "MyGNC") {
        // 声明输入：来自动力学的位置和速度
        declareInput<std::vector<double>>("position_truth", {{id, "Dynamics"}, "position_truth_m"});
        declareInput<std::vector<double>>("velocity_truth", {{id, "Dynamics"}, "velocity_truth_mps"});
        
        // 声明输出：制导指令
        declareOutput<std::vector<double>>("desired_acceleration");
    }

protected:
    void updateImpl() override {
        using namespace gnc::coordination;
        
        try {
            // 获取全局变换提供者
            auto& provider = GET_GLOBAL_TRANSFORM_PROVIDER();
            
            // 获取当前状态（在惯性坐标系中）
            auto pos_data = getState<std::vector<double>>("position_truth");
            auto vel_data = getState<std::vector<double>>("velocity_truth");
            
            // 创建坐标系感知的张量
            auto position_inertial = makePositionFromStdVector(pos_data, "INERTIAL", provider);
            auto velocity_inertial = makeVelocityFromStdVector(vel_data, "INERTIAL", provider);
            
            // 转换到载体坐标系进行计算
            auto position_body = position_inertial.to("BODY");
            auto velocity_body = velocity_inertial.to("BODY");
            
            // 在载体坐标系中进行制导计算
            Vector3d desired_accel_body = computeGuidanceCommand(position_body.data(), velocity_body.data());
            
            // 转换回惯性坐标系输出
            auto desired_accel_inertial = makeVector3d(desired_accel_body, "BODY", provider).to("INERTIAL");
            
            // 输出结果
            setState("desired_acceleration", toStdVector(desired_accel_inertial));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Coordination transform error: {}", e.what());
            // 输出零指令作为安全回退
            setState("desired_acceleration", std::vector<double>{0.0, 0.0, 0.0});
        }
    }

private:
    Vector3d computeGuidanceCommand(const Vector3d& pos, const Vector3d& vel) {
        // 您的制导算法实现
        // 这里只是示例
        Vector3d target_pos(1000.0, 0.0, 0.0);  // 目标位置
        Vector3d error = target_pos - pos;
        Vector3d desired_accel = 0.1 * error - 0.05 * vel;  // 简单PD控制
        return desired_accel;
    }
};
```

## 示例：动力学组件输出载体系速度

在 `RigidBodyDynamics6DoF` 组件中，使用超简化接口只需一行代码：

```cpp
void updateImpl() override {
    // 更新惯性系状态
    setState("position_truth_m", position_);
    setState("velocity_truth_mps", velocity_);
    setState("attitude_truth_quat", attitude_);
    
    // 一行代码搞定坐标转换！
    setState("velocity_body_mps", SAFE_TRANSFORM_VEC(velocity_, "INERTIAL", "BODY"));
}
```

## 超简化使用方式

### 1. 最简单的用法（推荐）

```cpp
// 转换std::vector - 最常用
auto body_vel = TRANSFORM_VEC(inertial_vel, "INERTIAL", "BODY");

// 安全转换（失败时返回原值）
auto body_vel = SAFE_TRANSFORM_VEC(inertial_vel, "INERTIAL", "BODY");

// 转换Eigen向量
Vector3d force_body = TRANSFORM(force_inertial, "INERTIAL", "BODY");

// 安全转换Eigen向量
Vector3d force_body = SAFE_TRANSFORM(force_inertial, "INERTIAL", "BODY");
```

### 2. 函数接口（更灵活）

```cpp
// 转换std::vector
auto vel_body = transformVector(velocity_vec, "INERTIAL", "BODY");

// 转换任意张量类型
Vector3d pos_body = transform(position_inertial, "INERTIAL", "BODY");

// 如果已有FrameTensor，直接转换
auto force_ned = transform(force_body_tensor, "NED");
```

### 3. 在组件中的典型用法

```cpp
class MyComponent : public states::ComponentBase {
protected:
    void updateImpl() override {
        // 获取输入
        auto pos = getState<std::vector<double>>("position");
        auto vel = getState<std::vector<double>>("velocity");
        
        // 一行代码转换坐标系
        auto pos_body = TRANSFORM_VEC(pos, "INERTIAL", "BODY");
        auto vel_body = TRANSFORM_VEC(vel, "INERTIAL", "BODY");
        
        // 使用转换后的数据进行计算
        auto cmd = computeCommand(pos_body, vel_body);
        
        // 转回惯性系输出
        setState("command", TRANSFORM_VEC(cmd, "BODY", "INERTIAL"));
    }
};
```

## 高级用法

### 1. 添加自定义坐标系变换

如果您需要添加项目特定的坐标系变换：

```cpp
// 在初始化后添加自定义变换
void setupCustomTransforms() {
    auto& provider = gnc::components::utility::CoordinationInitializer::getGlobalGncProvider();
    
    // 添加静态变换（如：特殊传感器安装角度）
    auto sensor_transform = gnc::coordination::makeTransformFromEuler(0.1, 0.05, 0.02);
    provider.addUserStaticTransform("SENSOR_SPECIAL", "BODY", sensor_transform, 
                                   "Special sensor mounting");
    
    // 添加动态变换（如：云台指向）
    provider.addUserDynamicTransform("GIMBAL", "BODY", 
        []() -> gnc::coordination::Transform {
            // 获取云台当前角度并计算变换
            double gimbal_azimuth = getCurrentGimbalAzimuth();
            double gimbal_elevation = getCurrentGimbalElevation();
            return gnc::coordination::makeTransformFromEuler(0, gimbal_elevation, gimbal_azimuth);
        }, "Gimbal pointing transform");
}
```

### 2. 性能优化建议

```cpp
class OptimizedComponent : public states::ComponentBase {
private:
    // 缓存变换提供者引用，避免重复查找
    const gnc::coordination::ITransformProvider* provider_cache_ = nullptr;
    
public:
    void initialize() override {
        // 在初始化时缓存提供者引用
        provider_cache_ = &gnc::components::utility::CoordinationInitializer::getGlobalProvider();
    }
    
protected:
    void updateImpl() override {
        // 使用缓存的引用
        auto force_body = gnc::coordination::makeVector3d(
            Vector3d(100.0, 0.0, 0.0), "BODY", *provider_cache_);
        auto force_inertial = force_body.to("INERTIAL");
        
        // 进行计算...
    }
};
```

### 3. 调试和诊断

```cpp
void debugCoordinationSystem() {
    using namespace gnc::coordination;
    
    if (gnc::components::utility::CoordinationInitializer::isGlobalProviderAvailable()) {
        auto& provider = gnc::components::utility::CoordinationInitializer::getGlobalGncProvider();
        
        // 打印系统信息
        printCoordinateSystemInfo(provider);
        
        // 测试特定变换
        testCoordinateTransform(provider, "BODY", "INERTIAL");
        testCoordinateTransform(provider, "NED", "ECEF");
        
        // 验证系统完整性
        auto [is_valid, error_msg] = provider.validateTransforms();
        if (!is_valid) {
            std::cout << "Validation errors:\n" << error_msg << std::endl;
        }
    }
}
```

## 最佳实践

### 1. 坐标系命名约定

```cpp
// 推荐的坐标系命名
const char* INERTIAL = "INERTIAL";        // 惯性坐标系
const char* BODY = "BODY";                 // 载体坐标系
const char* NED = "NED";                   // 北东地坐标系
const char* SENSOR_GPS = "GPS";            // GPS传感器
const char* SENSOR_IMU = "IMU";            // IMU传感器
const char* TARGET_1 = "TARGET_1";         // 目标1
```

### 2. 错误处理

```cpp
void safeCoordinateTransform() {
    try {
        auto& provider = GET_GLOBAL_TRANSFORM_PROVIDER();
        
        // 检查变换是否存在
        if (provider.hasTransform("BODY", "TARGET")) {
            auto vec_body = MAKE_GLOBAL_VECTOR3D(Vector3d(1, 0, 0), "BODY");
            auto vec_target = vec_body.to("TARGET");
            // 使用转换结果...
        } else {
            LOG_WARN("Transform from BODY to TARGET not available");
            // 使用替代逻辑...
        }
        
    } catch (const gnc::coordination::TransformNotFoundError& e) {
        LOG_ERROR("Transform not found: {} -> {}", e.getFromFrame(), e.getToFrame());
    } catch (const std::exception& e) {
        LOG_ERROR("Coordinate transform error: {}", e.what());
    }
}
```

### 3. 单元测试支持

```cpp
// 为测试创建模拟的变换提供者
class MockTransformProvider : public gnc::coordination::ITransformProvider {
public:
    gnc::coordination::Transform getTransform(
        const gnc::coordination::FrameIdentifier& from,
        const gnc::coordination::FrameIdentifier& to) const override {
        // 返回预设的测试变换
        return gnc::coordination::Transform::Identity();
    }
    
    // 实现其他必要方法...
};

void testMyComponent() {
    MockTransformProvider mock_provider;
    
    // 使用模拟提供者测试组件
    auto test_vector = gnc::coordination::makeVector3d(
        Vector3d(1, 0, 0), "BODY", mock_provider);
    
    // 执行测试...
}
```

## 常见问题

### Q: 如何添加新的坐标系？

A: 在`GncTransformProvider::initializeTransforms()`中添加相应的变换关系，或者在运行时使用`addUserStaticTransform`/`addUserDynamicTransform`方法。

### Q: 性能如何？

A: 系统支持变换缓存，同一仿真步内的重复变换会被缓存。对于高频调用，建议缓存变换提供者引用。

### Q: 如何处理变换不存在的情况？

A: 使用`hasTransform`方法检查，或者使用try-catch捕获`TransformNotFoundError`异常。

### Q: 为什么不需要手动设置StateManager？

A: 框架的自动注册机制已经处理了组件与StateManager的关联。每个组件都可以通过继承的`getState`/`setState`方法访问状态。