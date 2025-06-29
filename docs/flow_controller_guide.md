# 流程控制组件开发文档

## 1. 概述

流程控制组件是一类可以计算流程状态的特殊组件，用于让用户便捷地进行流程控制而不需要写复杂的逻辑分支代码。这类组件通常不作为单独的组件使用，而是作为其它组件的一个属性，用于管理组件内部的状态转换逻辑。

### 1.1 应用场景

流程控制组件适用于以下场景：

- **飞行阶段管理**：管理飞行器在不同阶段（地面、起飞、爬升、巡航、下降、着陆）的状态转换
- **制导律切换**：根据不同的飞行状态切换不同的制导律
- **导航模式切换**：根据传感器可用性和飞行阶段切换不同的导航模式
- **控制模式切换**：在姿态控制、速度控制、位置控制等模式之间切换
- **故障处理流程**：管理故障检测、诊断和恢复的流程
- **任务规划**：管理复杂任务的执行流程

### 1.2 设计目标

- **简化流程控制**：提供简洁的接口，减少条件分支代码
- **状态管理**：清晰地定义和管理状态及其转换条件
- **可扩展性**：易于扩展和定制新的流程控制逻辑
- **可重用性**：可以在不同组件中重用相同的流程控制逻辑

## 2. 核心组件

### 2.1 FlowController

`FlowController` 是流程控制组件的核心类，它实现了一个通用的状态机，用于管理状态转换逻辑。

#### 2.1.1 主要特性

- **状态定义**：支持自定义状态和初始状态
- **转换条件**：基于条件函数的状态转换
- **状态动作**：进入状态、离开状态和状态内动作
- **事件驱动**：支持外部事件触发状态转换
- **历史记录**：记录状态转换历史

#### 2.1.2 类图

```
+------------------+
| FlowController    |
+------------------+
| - current_state_  |
| - previous_state_ |
| - states_         |
| - transitions_    |
+------------------+
| + addState()      |
| + addTransition() |
| + update()        |
| + forceTransition()|
| + triggerEvent()  |
+------------------+
```



## 3. 接口设计

### 3.1 FlowController 接口

#### 3.1.1 构造函数

```cpp
FlowController(states::VehicleId id, const std::string& name, const StateType& initial_state);
```

- `id`：飞行器ID
- `name`：组件名称
- `initial_state`：初始状态名称

#### 3.1.2 状态管理

```cpp
FlowController& addState(const State& state);
FlowController& addState(const StateType& name, const std::string& description = "");
```

- `state`：状态定义
- `name`：状态名称
- `description`：状态描述

#### 3.1.3 转换管理

```cpp
FlowController& addTransition(const Transition& transition);
FlowController& addTransition(
    const StateType& from_state,
    const StateType& to_state,
    ConditionFunc condition,
    const std::string& description = ""
);
```

- `transition`：转换定义
- `from_state`：源状态
- `to_state`：目标状态
- `condition`：转换条件函数
- `description`：转换描述

#### 3.1.4 事件管理

```cpp
bool triggerEvent(const std::string& event_name);
FlowController& addEventTransition(
    const std::string& event_name,
    const StateType& from_state,
    const StateType& to_state
);
```

- `event_name`：事件名称
- `from_state`：源状态
- `to_state`：目标状态

#### 3.1.5 状态动作

```cpp
FlowController& setEntryAction(const StateType& state, ActionFunc action);
FlowController& setExitAction(const StateType& state, ActionFunc action);
FlowController& setUpdateAction(const StateType& state, ActionFunc action);
```

- `state`：状态名称
- `action`：动作函数

#### 3.1.6 状态查询

```cpp
const StateType& getCurrentState() const;
const StateType& getPreviousState() const;
double getTimeInState() const;
bool hasStateChanged() const;
const std::vector<StateTransitionRecord>& getStateHistory() const;
```



## 4. 使用方法

### 4.1 基本用法

#### 4.1.1 创建流程控制器

```cpp
// 创建流程控制器
auto flow_controller = std::make_unique<FlowController>(vehicle_id, "MyFlowController", "initial_state");

// 添加状态
flow_controller->addState("initial_state", "Initial state description")
               .addState("state_1", "State 1 description")
               .addState("state_2", "State 2 description");

// 添加转换条件
flow_controller->addTransition("initial_state", "state_1", [this]() {
    return some_condition;
}, "Transition to state 1");

flow_controller->addTransition("state_1", "state_2", [this]() {
    return another_condition;
}, "Transition to state 2");

// 添加事件转换
flow_controller->addEventTransition("emergency", "state_1", "initial_state");

// 设置状态动作
flow_controller->setEntryAction("state_1", [this]() {
    // 进入状态1时执行的动作
});

flow_controller->setExitAction("state_1", [this]() {
    // 离开状态1时执行的动作
});

flow_controller->setUpdateAction("state_1", [this]() {
    // 在状态1内每次更新时执行的动作
});
```

#### 4.1.2 在组件中使用流程控制器

```cpp
class MyComponent : public states::ComponentBase {
public:
    MyComponent(states::VehicleId id)
        : states::ComponentBase(id, "MyComponent") {
        
        // 声明输入输出状态
        declareInput<double>("input_1", {{id, "OtherComponent"}, "output_1"});
        declareOutput<double>("output_1");
        
        // 初始化流程控制器
        initFlowController();
    }

    std::string getComponentType() const override {
        return "MyComponent";
    }

protected:
    void updateImpl() override {
        // 更新流程控制器
        flow_controller_->update();
        
        // 根据当前状态执行不同的逻辑
        if (flow_controller_->getCurrentState() == "state_1") {
            // 状态1的处理逻辑
        } else if (flow_controller_->getCurrentState() == "state_2") {
            // 状态2的处理逻辑
        }
        
        // 更新输出状态
        setState("output_1", calculated_value);
    }

private:
    void initFlowController() {
        // 创建并配置流程控制器
        flow_controller_ = std::make_unique<FlowController>(getVehicleId(), getName() + "_FlowController", "initial_state");
        
        // 添加状态和转换...
    }

private:
    std::unique_ptr<FlowController> flow_controller_;
};
```



### 4.2 示例：飞行阶段管理器

```cpp
class FlightPhaseManager : public states::ComponentBase {
public:
    enum class FlightPhase {
        GROUND,
        TAKEOFF,
        CLIMB,
        CRUISE,
        DESCENT,
        LANDING
    };
    
    FlightPhaseManager(states::VehicleId id)
        : states::ComponentBase(id, "FlightPhaseManager") {
        
        // 声明输入状态
        declareInput<double>("altitude", {{id, "Navigation"}, "altitude"});
        declareInput<double>("airspeed", {{id, "Navigation"}, "airspeed"});
        declareInput<bool>("on_ground", {{id, "Navigation"}, "on_ground"});
        
        // 声明输出状态
        declareOutput<std::string>("current_phase");
        declareOutput<int>("phase_id");
        
        // 初始化流程控制器
        initFlowController();
    }

protected:
    void updateImpl() override {
        // 更新流程控制器
        flow_controller_->update();
        
        // 更新输出状态
        setState("current_phase", flow_controller_->getCurrentState());
        setState("phase_id", static_cast<int>(getCurrentPhase()));
    }

private:
    void initFlowController() {
        // 创建流程控制器
        flow_controller_ = std::make_unique<FlowController>(getVehicleId(), getName() + "_FlowController", "ground");
        
        // 添加状态
        flow_controller_->addState("ground", "Aircraft on ground")
                        .addState("takeoff", "Aircraft taking off")
                        .addState("climb", "Aircraft climbing to cruise altitude")
                        .addState("cruise", "Aircraft at cruise altitude")
                        .addState("descent", "Aircraft descending")
                        .addState("landing", "Aircraft landing");
        
        // 添加转换条件
        flow_controller_->addTransition("ground", "takeoff", [this]() {
            return getState<double>("airspeed") > 50.0;
        }, "Takeoff speed reached");
        
        // 添加更多转换...
    }

private:
    std::unique_ptr<FlowController> flow_controller_;
};
```

## 5. 高级功能

### 5.1 条件组合

可以使用逻辑组合器来创建复杂的转换条件：

```cpp
// 与条件
auto condition_and = [this]() {
    return condition1() && condition2();
};

// 或条件
auto condition_or = [this]() {
    return condition1() || condition2();
};

// 非条件
auto condition_not = [this]() {
    return !condition1();
};

// 复杂条件
auto complex_condition = [this]() {
    return (condition1() && condition2()) || (condition3() && !condition4());
};
```

### 5.2 状态超时

可以实现基于时间的状态转换：

```cpp
flow_controller_->addTransition("state_1", "state_2", [this]() {
    return flow_controller_->getTimeInState() > 10.0; // 10秒后转换
}, "Timeout after 10 seconds");
```

### 5.3 状态历史

可以查询状态转换历史：

```cpp
auto history = flow_controller_->getStateHistory();
for (const auto& record : history) {
    LOG_INFO("Transition: {} -> {} ({})", 
             record.from_state, record.to_state, record.reason);
}
```

### 5.4 嵌套状态机

可以实现嵌套的状态机：

```cpp
class NestedFlowController : public FlowController {
public:
    NestedFlowController(states::VehicleId id, const std::string& name, const StateType& initial_state)
        : FlowController(id, name, initial_state) {
        
        // 为每个状态创建子状态机
        sub_controllers_["state_1"] = std::make_unique<FlowController>(id, name + "_State1", "sub_initial");
        // 配置子状态机...
    }

protected:
    void updateImpl() override {
        // 先更新主状态机
        FlowController::updateImpl();
        
        // 然后更新当前状态的子状态机
        const auto& current_state = getCurrentState();
        if (sub_controllers_.find(current_state) != sub_controllers_.end()) {
            sub_controllers_[current_state]->update();
        }
    }

private:
    std::unordered_map<StateType, std::unique_ptr<FlowController>> sub_controllers_;
};
```

## 6. 最佳实践

建议采用**混合设计模式**，具体如下：

### 推荐方案：组件内嵌 + 专用流程管理组件

#### 1. 对于简单流程管理的组件 - 使用内嵌属性

```cpp
class NavigationComponent : public ComponentBase {
private:
    std::unique_ptr<FlowController> flow_controller_;
    
public:
    NavigationComponent(VehicleId id) : ComponentBase(id, "Navigation") {
        // 初始化简单的导航状态流程
        flow_controller_ = std::make_unique<FlowController>(id, "NavFlow", "init");
        flow_controller_->addState("init", "Initialization")
                        .addState("calibrating", "Sensor Calibration")
                        .addState("ready", "Ready for Navigation")
                        .addState("navigating", "Active Navigation");
        
        // 添加简单转换逻辑
        flow_controller_->addTransition("init", "calibrating", [this]() {
            return sensors_initialized_;
        });
    }
};
```

**适用场景：**
- 流程相对简单（3-6个状态）
- 状态转换逻辑主要依赖组件内部数据
- 不需要与其他组件频繁交互

#### 2. 对于复杂流程管理 - 使用专用流程管理组件

```cpp
class GuidancePhaseManager : public ComponentBase {
private:
    std::unique_ptr<FlowController> flow_controller_;
    
public:
    GuidancePhaseManager(VehicleId id) : ComponentBase(id, "GuidancePhaseManager") {
        // 声明输入状态（来自其他组件）
        declareInput<double>("altitude", {{id, "Navigation"}, "altitude"});
        declareInput<double>("target_distance", {{id, "Guidance"}, "target_distance"});
        declareInput<std::string>("mission_phase", {{id, "Mission"}, "current_phase"});
        
        // 声明输出状态（供其他组件使用）
        declareOutput<std::string>("guidance_mode");
        declareOutput<bool>("mode_changed");
        
        initComplexFlowController();
    }
    
private:
    void initComplexFlowController() {
        flow_controller_ = std::make_unique<FlowController>(getVehicleId(), "GuidanceFlow", "standby");
        
        // 复杂的制导阶段状态
        flow_controller_->addState("standby", "Standby Mode")
                        .addState("ascent_guidance", "Ascent Guidance")
                        .addState("midcourse_guidance", "Midcourse Guidance")
                        .addState("terminal_guidance", "Terminal Guidance")
                        .addState("impact_guidance", "Impact Guidance");
        
        // 复杂的跨组件转换逻辑
        flow_controller_->addTransition("standby", "ascent_guidance", [this]() {
            return getState<std::string>("mission_phase") == "launch" && 
                   getState<double>("altitude") > 100.0;
        });
    }
};
```

**适用场景：**
- 流程复杂（7+个状态）
- 需要跨组件数据进行状态转换
- 流程逻辑可能被多个组件共享
- 需要独立的流程监控和调试

### 设计原则建议

#### 1. **单一职责原则**
- 核心功能组件专注于自身算法实现
- 复杂流程管理独立为专门组件

#### 2. **依赖倒置原则**
```cpp
// 制导组件依赖流程管理组件的输出
class GuidanceLogic : public ComponentBase {
public:
    GuidanceLogic(VehicleId id) : ComponentBase(id, "GuidanceLogic") {
        declareInput<std::string>("guidance_mode", {{id, "GuidancePhaseManager"}, "guidance_mode"});
    }
    
    void updateImpl() override {
        std::string mode = getState<std::string>("guidance_mode");
        
        if (mode == "ascent_guidance") {
            performAscentGuidance();
        } else if (mode == "terminal_guidance") {
            performTerminalGuidance();
        }
    }
};
```

#### 3. **配置灵活性**
```cpp
// 在配置文件中定义组件关系
// config/logic.yaml
components:
  - type: "NavigationComponent"  # 内嵌简单流程
    id: "nav_001"
  - type: "GuidancePhaseManager"  # 专用流程管理
    id: "guidance_phase_001"
  - type: "GuidanceLogic"  # 依赖流程管理的功能组件
    id: "guidance_001"
```

### 具体建议

1. **导航组件**：使用内嵌 FlowController
   - 状态：初始化 → 校准 → 就绪 → 导航中
   - 逻辑相对简单，主要依赖传感器状态

2. **制导组件**：使用专用 GuidancePhaseManager
   - 状态：待机 → 上升段制导 → 中段制导 → 末段制导
   - 需要综合多个组件的信息进行决策

3. **气动组件**：使用内嵌 FlowController
   - 状态：静态 → 亚音速 → 跨音速 → 超音速
   - 主要依赖飞行速度和高度

这种混合设计既保持了简单组件的内聚性，又为复杂流程提供了足够的灵活性和可维护性。
        