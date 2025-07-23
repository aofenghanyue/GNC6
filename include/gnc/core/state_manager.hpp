#pragma once
#include "../common/types.hpp"
#include "component_base.hpp"
#include "../common/exceptions.hpp"
#include "state_access.hpp"
#include "../../math/math.hpp"  // 添加数学类型支持
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <any>
#include <vector>
#include <functional> // for std::function in topological sort
#include "../components/utility/simple_logger.hpp"

namespace gnc {

// 使用别名以反映其元框架特性
using namespace states;

/**
 * @brief 状态管理器，元框架的核心。
 * @details 负责组件的生命周期、状态数据的存储，以及通过依赖分析自动确定执行顺序。
 */
class StateManager : public IStateAccess {
public:
    ~StateManager() {
        // 按执行顺序的逆序终结组件，确保依赖项最后被清理
        std::vector<ComponentId> reverse_order = executionOrder_;
        std::reverse(reverse_order.begin(), reverse_order.end());
        
        LOG_INFO("[StateManager] Finalizing components in reverse order...");
        for (const auto& id : reverse_order) {
            if (components_.count(id)) {
                LOG_DEBUG("[Finalize] -> {}", id.name.c_str());
                components_.at(id)->finalize();
            }
        }

        // 清理资源
        for (auto& [id, component] : components_) {
            if (component) {
                component->setStateAccess(nullptr);
                delete component;
            }
        }
        components_.clear();
        states_.clear();
        LOG_INFO("[StateManager] Shutdown complete.");
    }

    void registerComponent(ComponentBase* component) {
        if (!component) return;
        auto id = component->getComponentId();
        if (components_.count(id)) {
            throw ConfigurationError("StateManager", "Component '" + id.name + "' already registered.");
        }

        try {
            component->getInterface().validate();
        } catch (const std::exception& e) {
            throw ConfigurationError("StateManager", "Component '" + id.name + "' interface validation failed: " + e.what());
        }

        auto interface = component->getInterface();
        for (const auto& spec : interface.getOutputs()) {
            states_[StateId{id, spec.name}] = spec.default_value.has_value() ? std::any(spec.default_value) : std::any();
        }

        component->setStateAccess(this);
        components_[id] = component;
        LOG_INFO("[StateManager] Registered component: {}-{}", id.vehicleId, id.name.c_str());
        needsRevalidation_ = true;
    }

    void validateAndSortComponents() {
        if (!needsRevalidation_) return;

        LOG_DEBUG("[StateManager] Validating dependencies and performing topological sort...");
        
        // 1. 构建依赖图
        std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>> graph;
        for (const auto& [id, component] : components_) {
            graph[id] = {}; // 确保每个组件都是图中的一个节点
            auto interface = component->getInterface();
            for (const auto& inputSpec : interface.getInputs()) {
                if(inputSpec.source && inputSpec.required) {
                    graph[id].insert(inputSpec.source->component);
                }
            }
        }

        // 2. 拓扑排序 (使用DFS)
        std::vector<ComponentId> sorted_order;
        std::unordered_set<ComponentId, std::hash<ComponentId>> visited;
        std::unordered_set<ComponentId, std::hash<ComponentId>> recursion_stack;

        std::function<void(const ComponentId&)> dfs = 
            [&](const ComponentId& u) {
            visited.insert(u);
            recursion_stack.insert(u);

            if (graph.count(u)) {
                for (const auto& v : graph.at(u)) {
                    if (recursion_stack.count(v)) {
                        throw DependencyError("StateManager", "Cyclic dependency detected involving " + v.name);
                    }
                    if (!visited.count(v)) {
                        dfs(v);
                    }
                }
            }
            recursion_stack.erase(u);
            sorted_order.push_back(u);
        };

        for (const auto& [id, _] : components_) {
            if (!visited.count(id)) {
                dfs(id);
            }
        }
        
        executionOrder_ = sorted_order;

        LOG_DEBUG("[StateManager] Component execution order determined");
        
        // 3.检测拓扑排序后的组件是否都已注册
        std::vector<std::string> unregistered_components;
        for(const auto& id : executionOrder_) {
            if (components_.find(id) == components_.end()) {
                unregistered_components.push_back(id.name);
            }
        }
        
        if (!unregistered_components.empty()) {
            std::string error_msg = "The following components in execution order are not registered: ";
            for (size_t i = 0; i < unregistered_components.size(); ++i) {
                error_msg += unregistered_components[i];
                if (i < unregistered_components.size() - 1) {
                    error_msg += ", ";
                }
            }
            LOG_ERROR("[StateManager] {}", error_msg.c_str());
            throw ConfigurationError("StateManager", error_msg);
        }
        
        // 多行输出执行链条，保持视觉连贯性
        LOG_INFO("[StateManager] Execution chain:");
        LOG_INFO("  ┌─ LOOP_START");
        for(const auto& id : executionOrder_) {
            LOG_INFO("  ├─ {}", id.name.c_str());
        }
        LOG_INFO("  └─ LOOP_END");
        
        LOG_INFO("[StateManager] All components in execution order are properly registered.");

        // 4. 状态连接验证 (新增)
        validateStateConnections();

        // 5. 初始化所有组件
        LOG_INFO("[StateManager] Initializing components...");
        for (const auto& id : executionOrder_) {
            if (components_.count(id)) {
                LOG_DEBUG("[Initialize] -> {}", id.name.c_str());
                components_.at(id)->initialize();
            }
        }

        needsRevalidation_ = false;
    }

    void updateAll() {
        if (needsRevalidation_) {
            validateAndSortComponents();
        }
        for (const auto& id : executionOrder_) {
            if (components_.count(id)) {
                LOG_TRACE("[Update] -> {}", id.name.c_str());
                components_.at(id)->update();
            }
        }
    }

    /**
     * @brief 获取所有已注册的组件ID列表
     * @return 所有已注册组件的ID向量
     */
    std::vector<ComponentId> getAllComponentIds() const {
        std::vector<ComponentId> component_ids;
        component_ids.reserve(components_.size());
        for (const auto& [id, component] : components_) {
            component_ids.push_back(id);
        }
        return component_ids;
    }

    /**
     * @brief 获取指定组件的所有输出状态ID列表
     * @param component_id 组件ID
     * @return 该组件的所有输出状态ID向量
     */
    std::vector<StateId> getComponentOutputStates(const ComponentId& component_id) const {
        std::vector<StateId> output_states;
        auto it = components_.find(component_id);
        if (it != components_.end()) {
            auto interface = it->second->getInterface();
            for (const auto& output : interface.getOutputs()) {
                output_states.push_back(StateId{component_id, output.name});
            }
        }
        return output_states;
    }

    /**
     * @brief 获取所有已注册的输出状态ID列表
     * @return 所有输出状态ID向量
     */
    std::vector<StateId> getAllOutputStates() const {
        std::vector<StateId> all_states;
        for (const auto& [component_id, component] : components_) {
            auto interface = component->getInterface();
            for (const auto& output : interface.getOutputs()) {
                all_states.push_back(StateId{component_id, output.name});
            }
        }
        return all_states;
    }

    /**
     * @brief 获取状态的类型信息
     * @param state_id 状态标识符
     * @return 状态的类型名称字符串，如果状态不存在则返回空字符串
     */
    std::string getStateType(const StateId& state_id) const {
        auto it = components_.find(state_id.component);
        if (it != components_.end()) {
            auto interface = it->second->getInterface();
            for (const auto& output : interface.getOutputs()) {
                if (output.name == state_id.name) {
                    return output.type;
                }
            }
        }
        return "";
    }

    /**
     * @brief 获取状态的原始std::any值
     * @param state_id 状态标识符
     * @return 状态的std::any值引用
     * @throws StateAccessError 如果状态不存在
     */
    const std::any& getRawStateValue(const StateId& state_id) const {
        auto it = states_.find(state_id);
        if (it != states_.end()) {
            return it->second;
        }
        throw StateAccessError("StateManager", "State '" + state_id.name + "' not found for component '" + state_id.component.name + "'.");
    }

protected:
    const std::any& getStateImpl(const StateId& id, const std::string& type) const override {
        if (states_.count(id)) {
            const auto& value = states_.at(id);
            if (value.type() == typeid(void)) {
                 throw StateAccessError("StateManager", "State '" + id.name + "' of component '" + id.component.name + "' has not been initialized (is empty).");
            }
            // 运行时类型检查（虽然any_cast也会做）
            if (value.type().name() != type) {
                 // 注意：typeid().name() 的结果是实现定义的，可能不美观，但可用于调试
                 throw StateAccessError("StateManager", "Type mismatch for state '" + id.name + "'. Requested " + type + " but has " + value.type().name());
            }
            return value;
        }
        throw StateAccessError("StateManager", "State '" + id.name + "' not found for component '" + id.component.name + "'.");
    }

    void setStateImpl(const StateId& id, const std::any& value, const std::string& /* type */) override {
         if (states_.count(id)) {
            states_[id] = value;
        } else {
            // 原则上不应该发生，因为输出状态在注册时已创建
            throw StateAccessError("StateManager", "Attempt to set an undeclared output state '" + id.name + "'.");
        }
    }

private:
    /**
     * @brief 验证状态连接的完整性和类型匹配
     * @details 在组件初始化前检查所有输入状态的连接，包括：
     * 1. 检查源状态是否存在
     * 2. 检查类型是否匹配
     * 3. 生成友好的错误报告
     * @throws ConfigurationError 当发现连接错误时抛出
     */
    void validateStateConnections() {
        LOG_DEBUG("[StateManager] Validating state connections...");
        
        // 收集所有输出状态的类型信息
        std::unordered_map<StateId, std::string, std::hash<StateId>> outputTypes;
        for (const auto& [id, component] : components_) {
            auto interface = component->getInterface();
            for (const auto& output : interface.getOutputs()) {
                StateId stateId{id, output.name};
                outputTypes[stateId] = output.type;
            }
        }
        
        // 验证所有输入状态的连接
        std::vector<std::string> errors;
        for (const auto& [id, component] : components_) {
            auto interface = component->getInterface();
            for (const auto& input : interface.getInputs()) {
                if (!input.source) continue; // 跳过没有源的输入
                
                // 检查源状态是否存在
                auto it = outputTypes.find(*input.source);
                if (it == outputTypes.end()) {
                    errors.push_back(formatConnectionError(id, input, "source state not found"));
                    continue;
                }
                
                // 检查类型是否匹配
                if (it->second != input.type) {
                    errors.push_back(formatTypeError(id, input, it->second));
                }
            }
        }
        
        // 如果有错误，生成报告并抛出异常
        if (!errors.empty()) {
            LOG_ERROR("[StateManager] State connection validation failed with {} errors:", errors.size());
            for (size_t i = 0; i < errors.size(); ++i) {
                LOG_ERROR("  [{}] {}", i + 1, errors[i].c_str());
            }
            
            std::string errorMsg = "State connection validation failed:\n";
            for (const auto& error : errors) {
                errorMsg += "  - " + error + "\n";
            }
            throw ConfigurationError("StateManager", errorMsg);
        }
        
        LOG_INFO("[StateManager] State connection validation passed successfully");
    }
    
    /**
     * @brief 格式化连接错误信息
     */
    std::string formatConnectionError(const ComponentId& component, 
                                     const StateSpec& input,
                                     const std::string& reason) {
        return "Component '" + component.name + 
               "' input '" + input.name + 
               "' cannot connect to '" + input.source->component.name + 
               "." + input.source->name + "': " + reason;
    }
    
    /**
     * @brief 格式化类型错误信息
     */
    std::string formatTypeError(const ComponentId& component,
                               const StateSpec& input,
                               const std::string& actualType) {
        std::string expectedType = friendlyTypeName(input.type);
        std::string foundType = friendlyTypeName(actualType);
        
        return "Type mismatch: Component '" + component.name + 
               "' input '" + input.name + 
               "' expects type '" + expectedType + 
               "' but '" + input.source->component.name + 
               "." + input.source->name + 
               "' provides type '" + foundType + "'";
    }
    
    /**
     * @brief 将编译器类型名转换为友好的类型名
     */
    std::string friendlyTypeName(const std::string& mangledName) {
        // 简单的映射表，用于常见的GNC类型
        static const std::unordered_map<std::string, std::string> typeMap = {
            {typeid(Vector3d).name(), "Vector3d"},
            {typeid(Quaterniond).name(), "Quaterniond"}, 
            {typeid(double).name(), "double"},
            {typeid(float).name(), "float"},
            {typeid(int).name(), "int"},
            {typeid(bool).name(), "bool"},
            {typeid(Matrix3d).name(), "Matrix3d"},
            {typeid(std::string).name(), "string"}
        };
        
        auto it = typeMap.find(mangledName);
        if (it != typeMap.end()) {
            return it->second;
        }
        
        // 如果没有找到映射，返回原始名称（可能不够友好，但至少可用）
        return mangledName;
    }

    std::unordered_map<ComponentId, ComponentBase*, std::hash<ComponentId>> components_;
    std::unordered_map<StateId, std::any, std::hash<StateId>> states_;
    std::vector<ComponentId> executionOrder_;
    bool needsRevalidation_{true};
};

} // namespace gnc