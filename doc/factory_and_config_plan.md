# 设计方案：通过组件工厂和配置文件优化main函数

本文档旨在解决当前`main.cpp`中硬编码组件创建流程的问题，提出一个基于组件工厂和配置文件驱动的自动化方案。

## 1. 问题分析

当前`main.cpp`通过 `new ComponentType(...)` 的方式直接创建组件实例，并注册到`StateManager`。这种方式存在以下缺点：

- **缺乏灵活性**: 每当需要增删或替换组件时，都必须修改并重新编译`main.cpp`。
- **对用户不友好**: 框架使用者被迫要理解并修改框架的核心启动流程，而不是专注于组件开发。
- **可扩展性差**: 无法动态地根据场景配置不同的组件组合。

## 2. 核心设计：组件工厂与自注册

为了解决C++缺乏原生反射机制的问题，我们将采用“工厂模式”和“静态自注册”相结合的经典方案。

- **组件工厂 (`ComponentFactory`)**: 一个中心化的服务，负责根据字符串标识（组件类型）创建对应的组件实例。
- **自注册 (`ComponentRegistrar`)**: 一个辅助模板类，利用C++静态对象在`main`函数执行前初始化的特性，让每个组件自动向工厂“报到”，注册自己的创建方法。

## 3. 具体实施步骤

### 步骤1：创建 `ComponentFactory`

我们将创建一个新的单例类 `ComponentFactory`，负责存储“创建者函数”并按需调用。

```cpp
// file: include/gnc/core/component_factory.hpp
#pragma once

#include "component_base.hpp"
#include "../common/exceptions.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace gnc {

// 创建者函数的类型定义：接受一个VehicleId，返回一个ComponentBase指针
using ComponentCreator = std::function<states::ComponentBase*(states::VehicleId)>;

class ComponentFactory {
public:
    static ComponentFactory& getInstance() {
        static ComponentFactory instance;
        return instance;
    }

    // 禁止拷贝和移动
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

    /**
     * @brief 注册一个组件创建者
     * @param type 组件的类型字符串
     * @param creator 创建该组件的函数
     */
    void registerCreator(const std::string& type, ComponentCreator creator) {
        if (creators_.find(type) != creators_.end()) {
            // 可以根据需要决定是抛出异常还是仅记录警告
            LOG_WARN("[Factory] Component type '{}' is already registered. Overwriting.", type);
        }
        creators_[type] = std::move(creator);
    }

    /**
     * @brief 根据类型字符串创建组件实例
     * @param type 要创建的组件类型
     * @param id 飞行器ID
     * @return 指向新创建组件的指针，如果类型未注册则返回nullptr
     */
    states::ComponentBase* createComponent(const std::string& type, states::VehicleId id) {
        auto it = creators_.find(type);
        if (it == creators_.end()) {
            LOG_ERROR("[Factory] Component type '{}' not found.", type);
            throw gnc::ConfigurationError("ComponentFactory", "Component type '" + type + "' not registered.");
        }
        return it->second(id);
    }

private:
    ComponentFactory() = default;
    ~ComponentFactory() = default;

    std::unordered_map<std::string, ComponentCreator> creators_;
};

}
```

### 步骤2：创建自注册辅助类 `ComponentRegistrar`

这个小巧的模板类是实现自动化的关键。

```cpp
// file: include/gnc/core/component_registrar.hpp
#pragma once

#include "component_factory.hpp"
#include <string>

namespace gnc {

template <typename T>
class ComponentRegistrar {
public:
    /**
     * @brief 构造函数会在main之前被调用，从而实现自注册
     * @param type 要注册的组件类型字符串
     */
    explicit ComponentRegistrar(const std::string& type) {
        // 获取工厂实例，并注册一个lambda函数作为创建者
        // 这个lambda函数知道如何创建类型为T的组件
        ComponentFactory::getInstance().registerCreator(type, 
            [](states::VehicleId id) -> states::ComponentBase* {
                return new T(id);
            }
        );
    }
};

}
```

### 步骤3：修改组件以实现自注册

现在，我们只需在每个组件的头文件中添加一行代码，即可将其接入新的自动化系统。

**示例：修改 `rigid_body_dynamics_6dof.hpp`**
```cpp
// file: include/gnc/components/dynamics/rigid_body_dynamics_6dof.hpp
#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp" // 1. 包含注册器头文件
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

class RigidBodyDynamics6DoF : public states::ComponentBase {
    // ... 类实现保持不变 ...
};

// 2. 在命名空间内，添加静态注册器实例
// 这行代码确保了RigidBodyDynamics6DoF在程序启动时会自动注册到工厂中
static gnc::ComponentRegistrar<RigidBodyDynamics6DoF> registrar("RigidBodyDynamics6DoF");

}
```
**注意**: 需要对所有现有的组件文件执行此项修改。

### 步骤4：定义 `core.yaml` 配置文件格式

我们将采用清晰的列表格式来定义仿真所需的组件。

```yaml
# file: config/core.yaml

simulation:
  vehicles:
    - id: 1  # 飞行器ID
      components:
        # 按期望的逻辑顺序或任意顺序列出组件
        # StateManager会自动处理依赖关系和执行顺序
        - SimpleAtmosphere
        - RigidBodyDynamics6DoF
        - SimpleAerodynamics
        - IdealIMUSensor
        - PerfectNavigation
        - GuidanceLogic
        - ControlLogic
        # - BiasAdapter # BiasAdapter构造函数需要额外参数，暂时注释

    # 可以轻松扩展到多飞行器
    # - id: 2
    #   components:
    #     - ...
```

### 步骤5：重构 `main.cpp`

最终，`main.cpp`将被极大简化，成为一个固定的、由配置驱动的启动器。

```cpp
// file: src/main.cpp
#include <iostream>
#include "gnc/core/state_manager.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/core/component_factory.hpp"

int main() {
    try {
        // 1. 初始化核心服务
        gnc::components::utility::ConfigManager::getInstance().loadConfigs("config/");
        gnc::components::utility::SimpleLogger::getInstance().initializeFromConfig();

        LOG_INFO("GNC Simulation Framework Initializing...");

        // 2. 创建状态管理器
        gnc::StateManager state_manager;

        // 3. 从配置加载并创建组件
        auto vehicles_config = gnc::components::utility::ConfigManager::getInstance()
                                .getComponentConfig("core", "simulation")["vehicles"];

        for (const auto& vehicle_config : vehicles_config) {
            gnc::states::VehicleId vehicle_id = vehicle_config["id"].get<gnc::states::VehicleId>();
            LOG_INFO("Loading components for Vehicle ID: {}", vehicle_id);

            for (const auto& comp_type : vehicle_config["components"]) {
                std::string type_str = comp_type.get<std::string>();
                LOG_DEBUG("Creating component of type: {}", type_str);
                
                // 使用工厂创建组件实例
                gnc::states::ComponentBase* component = gnc::ComponentFactory::getInstance().createComponent(type_str, vehicle_id);
                
                // 注册到状态管理器
                state_manager.registerComponent(component);
            }
        }

        // 4. 验证依赖关系并启动仿真循环
        state_manager.validateAndSortComponents();

        LOG_INFO("Starting simulation loop...");
        for (int i = 0; i < 1000; ++i) { // 示例循环
            state_manager.updateAll();
        }
        LOG_INFO("Simulation loop finished.");

    } catch (const std::exception& e) {
        LOG_CRITICAL("An unhandled exception occurred: {}", e.what());
        return 1;
    }

    return 0; // StateManager析构时会自动调用所有组件的finalize
}

```

## 4. 总结

该方案通过引入组件工厂和自注册机制，将`main.cpp`从一个具体的组件“编排者”转变为一个通用的、由配置驱动的“启动器”。这极大地提升了框架的灵活性和可扩展性，并显著改善了最终用户的开发体验，完美达成了设计目标。
