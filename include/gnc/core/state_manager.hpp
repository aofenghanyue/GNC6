#pragma once
#include "../common/types.hpp"
#include "component_base.hpp"
#include "../common/exceptions.hpp"
#include "state_access.hpp"
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
        
        // 多行输出执行链条，保持视觉连贯性
        LOG_INFO("[StateManager] Execution chain:");
        LOG_INFO("  ┌─ LOOP_START");
        for(const auto& id : executionOrder_) {
            LOG_INFO("  ├─ {}", id.name.c_str());
        }
        LOG_INFO("  └─ LOOP_END");

        // 3. 初始化所有组件
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
    std::unordered_map<ComponentId, ComponentBase*, std::hash<ComponentId>> components_;
    std::unordered_map<StateId, std::any, std::hash<StateId>> states_;
    std::vector<ComponentId> executionOrder_;
    bool needsRevalidation_{true};
};

} // namespace gnc