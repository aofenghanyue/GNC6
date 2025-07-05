/**
 * @file component_base.hpp
 * @brief 组件基类定义
 * 
 * @details 组件系统设计
 * 
 * 1. 核心功能
 *    - 状态声明：提供声明输入/输出状态的接口
 *    - 状态访问：封装状态的读写操作
 *    - 生命周期：管理组件的初始化、更新和销毁
 * 
 * 2. 设计模式
 *    - 模板方法模式：通过虚函数定义组件的标准接口
 *    - 依赖注入：通过 StateAccess 接口实现状态访问
 * 
 * 3. 使用方法
 *    @code
 *    class GPSSensor : public ComponentBase {
 *    public:
 *        GPSSensor(VehicleId id) : ComponentBase(id, "GPS") {
 *            // 声明输出状态
 *            declareOutput<Vector3d>("position");
 *            declareOutput<Vector3d>("velocity");
 *        }
 *    
 *    protected:
 *        void updateImpl() override {
 *            // 更新状态
 *            setState("position", getCurrentPosition());
 *            setState("velocity", getCurrentVelocity());
 *        }
 *    };
 *    @endcode
 * 
 * 4. 注意事项
 *    - 组件必须在构造函数中声明所有状态
 *    - 状态访问只能在组件注册后进行
 *    - 更新逻辑必须在 updateImpl() 中实现
 */
#pragma once

#include "../common/types.hpp"
#include "state_interface.hpp"
#include "state_access.hpp"
#include "../common/exceptions.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

namespace gnc {
    class StateManager;
}

namespace gnc::states {

/**
 * @brief 组件基类
 * 
 * @details 组件是状态系统的基本单元，负责：
 * 1. 状态管理
 *    - 声明组件的输入输出状态
 *    - 提供状态的读写接口
 *    - 维护状态的生命周期
 * 
 * 2. 更新机制
 *    - 定义标准的更新接口
 *    - 确保状态的同步更新
 *    - 处理更新中的异常
 * 
 * 3. 依赖管理
 *    - 通过状态声明建立组件间的依赖关系
 *    - 支持运行时的依赖检查
 *    - 确保依赖关系的正确性
 */
class ComponentBase {
public:
    ComponentBase(VehicleId vehicleId, std::string name)
        : vehicleId_{vehicleId}, name_{std::move(name)} {}
    
    // 支持可选实例名称的构造函数
    ComponentBase(VehicleId vehicleId, std::string defaultName, const std::string& instanceName)
        : vehicleId_{vehicleId}, name_{instanceName.empty() ? std::move(defaultName) : instanceName} {}

    virtual ~ComponentBase() = default;

    // 禁止拷贝和移动
    ComponentBase(const ComponentBase&) = delete;
    ComponentBase& operator=(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = delete;
    ComponentBase& operator=(ComponentBase&&) = delete;

    // --- 生命周期管理 (由 StateManager 调用) ---
    
    /**
     * @brief 初始化。在所有组件注册后、首次更新前调用。
     * 可选重写。用于执行一次性设置、资源分配等。
     */
    virtual void initialize() {}

    /**
     * @brief 更新。由StateManager在每个仿真步长中调用。
     */
    void update() {
        updateImpl();
    }

    /**
     * @brief 终结。在仿真结束或组件销毁前调用。
     * 可选重写。用于资源释放。
     */
    virtual void finalize() {}


    // --- 元数据与接口 ---

    /**
     * @brief 获取组件的唯一类型名称。
     * @return std::string 组件类型名，用于组件工厂。
     * @details 派生类必须实现此方法，返回一个唯一的字符串标识。
     */
    virtual std::string getComponentType() const = 0;

    /**
     * @brief 获取组件的状态接口
     * 
     * @return StateInterface 包含组件所有状态声明的接口对象
     * 
     * @details 状态接口用于：
     * 1. 组件注册时的依赖分析
     * 2. 运行时的状态访问验证
     * 3. 系统配置的完整性检查
     * 
     * 注意：这个接口会被状态管理器用来：
     * - 验证组件的配置
     * - 建立组件间的依赖关系
     * - 确定组件的更新顺序
     */
    virtual StateInterface getInterface() const {
        StateInterface interface;
        
        // 从存储中复制状态声明
        for (const auto& spec : stateSpecs_) {
            if (spec.access == StateAccessType::Input) {
                interface.addInput(spec);
            } else if (spec.access == StateAccessType::Output) {
                interface.addOutput(spec);
            }
        }
        
        return interface;
    }

    /**
     * @brief 获取组件名称
     */
    const std::string& getName() const { return name_; }

    /**
     * @brief 获取组件所属的飞行器ID
     */
    VehicleId getVehicleId() const { return vehicleId_; }

    /**
     * @brief 获取完整的组件ID
     */
    ComponentId getComponentId() const {
        return ComponentId{vehicleId_, getName()};
    }

    /**
     * @brief 获取状态访问接口
     * 
     * @return IStateAccess* 状态访问接口指针
     * 
     * @details 此方法用于：
     * 1. 将状态访问能力传递给工具组件
     * 2. 允许非注册组件访问状态管理器
     * 3. 支持组件内部的复杂状态管理
     */
    IStateAccess* getStateAccess() const {
        return stateAccess_;
    }

protected:
    /**
     * @brief 组件更新实现 (纯虚函数)
     * 
     * @details 组件的核心逻辑：
     * 1. 读取输入状态
     * 2. 执行组件特定的处理
     * 3. 更新输出状态
     * 
     * 派生类必须实现此方法，例如：
     * @code
     * void GPSSensor::updateImpl() {
     *     auto rawData = readSensor();
     *     auto position = processRawData(rawData);
     *     setState("position", position);
     * }
     * @endcode
     */
    virtual void updateImpl() = 0;

    /**
     * @brief 声明输入状态
     * 
     * @tparam T 状态的数据类型
     * @param name 状态名称
     * @param source 状态数据的来源，包含源组件ID和状态名
     * 
     * @details 输入状态声明过程：
     * 1. 创建状态规范(StateSpec)对象
     * 2. 记录状态类型、名称和来源
     * 3. 添加到组件的状态列表
     * 
     * 使用示例：
     * @code
     * declareInput<Vector3d>("gps_position", StateId{1, "GPS", "position"});
     * @endcode
     */
    template<typename T>
    void declareInput(const std::string& name, const StateId& source, bool required = true) {
        StateSpec spec{
            .name = name,
            .type = typeid(T).name(),
            .access = StateAccessType::Input,
            .source = source,
            .required = required,
            .default_value = std::any(),
        };
        stateSpecs_.push_back(spec);
    }

    /**
     * @brief 声明输出状态
     * 
     * @tparam T 状态的数据类型
     * @param name 状态名称
     * 
     * @details 输出状态声明过程：
     * 1. 创建状态规范(StateSpec)对象
     * 2. 设置状态类型和名称
     * 3. 添加到组件的状态列表
     * 
     * 使用示例：
     * @code
     * declareOutput<Vector3d>("position");
     * @endcode
     */
    template<typename T>
    void declareOutput(const std::string& name, const std::optional<T>& default_value = std::nullopt) {
        StateSpec spec{
            .name = name,
            .type = typeid(T).name(),
            .access = StateAccessType::Output,
            .source = std::nullopt,
            .required = true,
            .default_value = default_value.has_value() ? std::any(default_value.value()) : std::any(),
        };
        stateSpecs_.push_back(spec);
    }

    /**
     * @brief 获取状态值
     * 
     * @tparam T 状态的数据类型
     * @param name 状态名称
     * @return const T& 状态值的常量引用
     * @throws std::runtime_error 当组件未注册时抛出
     * 
     * @details 状态获取过程：
     * 1. 检查组件是否已注册
     * 2. 构造状态ID
     * 3. 通过状态访问接口获取值
     * 
     * 注意：只能在组件注册后使用此函数
     */
    template<typename T>
    const T& getState(const std::string& name) const {
        if (!stateAccess_) {
            throw std::runtime_error("Component not registered or StateManager no longer exists");
        }
        return stateAccess_->getState<T>(StateId{getComponentId(), name});
    }

    /**
     * @brief 获取指定StateId的状态值
     * 
     * @tparam T 状态的数据类型
     * @param stateId 完整的状态标识符
     * @return const T& 状态值的常量引用
     * @throws std::runtime_error 当组件未注册时抛出
     * 
     * @details 此方法用于：
     * 1. 访问其他组件的状态
     * 2. 实现跨组件的数据流
     * 3. 支持复杂的状态依赖
     */
    template<typename T>
    const T& getState(const StateId& stateId) const {
        if (!stateAccess_) {
            throw std::runtime_error("Component not registered or StateManager no longer exists");
        }
        return stateAccess_->getState<T>(stateId);
    }

    /**
     * @brief 设置状态值
     * 
     * @tparam T 状态的数据类型
     * @param name 状态名称
     * @param value 要设置的状态值
     * @throws std::runtime_error 当组件未注册时抛出
     * 
     * @details 状态设置过程：
     * 1. 检查组件是否已注册
     * 2. 构造状态ID
     * 3. 通过状态访问接口更新值
     * 
     * 注意：
     * - 只能在组件注册后使用此函数
     * - 只能设置本组件声明的输出状态
     */
    template<typename T>
    void setState(const std::string& name, const T& value) {
        if (!stateAccess_) {
            throw std::runtime_error("Component not registered or StateManager no longer exists");
        }
        stateAccess_->setState(StateId{getComponentId(), name}, value);
    }

    friend class gnc::StateManager;  // 允许 StateManager 访问 protected 成员

protected:
    void setStateAccess(IStateAccess* access) {
        stateAccess_ = access;
    }

private:

    VehicleId vehicleId_;
    std::string name_;
    IStateAccess* stateAccess_{nullptr};
    std::vector<StateSpec> stateSpecs_;
};

} // namespace gnc::states