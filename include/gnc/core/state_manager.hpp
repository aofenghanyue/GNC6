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
#include <queue> // for priority_queue in priority-aware sorting
#include "../components/utility/simple_logger.hpp"
#include "../components/utility/config_manager.hpp"

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
        componentDependencies_.clear();
        LOG_INFO("[StateManager] Shutdown complete.");
    }

    void registerComponent(ComponentBase* component, int priority = DEFAULT_PRIORITY) {
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
        
        // Extract component-level dependencies from simplified inputs
        std::unordered_set<ComponentId, std::hash<ComponentId>> componentDeps;
        for (const auto& spec : interface.getInputs()) {
            if (spec.source && spec.required) {
                componentDeps.insert(spec.source->component);
                LOG_DEBUG("[StateManager] Component {} declares dependency on {}", 
                         id.name.c_str(), spec.source->component.name.c_str());
            }
        }
        componentDependencies_[id] = componentDeps;
        
        // Store component priority
        componentPriorities_[id] = priority;
        
        // Initialize output states
        for (const auto& spec : interface.getOutputs()) {
            states_[StateId{id, spec.name}] = spec.default_value.has_value() ? std::any(spec.default_value) : std::any();
        }

        component->setStateAccess(this);
        components_[id] = component;
        LOG_INFO("[StateManager] Registered component: {}-{} with priority {}", id.vehicleId, id.name.c_str(), priority);
        needsRevalidation_ = true;
    }

    void validateAndSortComponents() {
        if (!needsRevalidation_) return;

        LOG_DEBUG("[StateManager] Validating dependencies and performing topological sort...");
        
        // 1. Component priorities are already loaded during registration
        
        // 2. Use pre-built component dependency graph
        std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>> graph;
        for (const auto& [id, component] : components_) {
            // Initialize with stored component dependencies
            if (componentDependencies_.count(id)) {
                graph[id] = componentDependencies_.at(id);
            } else {
                graph[id] = {}; // No dependencies
            }
        }

        // 3. 优先级感知的拓扑排序
        std::vector<ComponentId> sorted_order = performPriorityAwareTopologicalSort(graph);

        LOG_DEBUG("[StateManager] Component execution order determined");
        
        // 4.检测拓扑排序后的组件是否都已注册
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
        
        LOG_INFO("[StateManager] All components in execution order are properly registered.");

        // 5. 组件依赖验证 (增强)
        validateComponentDependencies();

        // 6. 初始化所有组件
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
     * @brief 验证组件依赖关系的完整性
     * @details 在组件初始化前检查所有组件级依赖，包括：
     * 1. 检查依赖的组件是否存在
     * 2. 生成清晰的错误报告
     * 3. 确保所有必需的组件依赖都能满足
     * @throws ConfigurationError 当发现依赖错误时抛出
     */
    void validateComponentDependencies() {
        LOG_DEBUG("[StateManager] Validating component dependencies...");
        
        // 收集所有已注册的组件ID
        std::unordered_set<ComponentId, std::hash<ComponentId>> registeredComponents;
        for (const auto& [id, component] : components_) {
            registeredComponents.insert(id);
        }
        
        // 验证所有组件的依赖关系
        std::vector<std::string> errors;
        for (const auto& [componentId, dependencies] : componentDependencies_) {
            for (const auto& dependencyId : dependencies) {
                // 检查依赖的组件是否存在
                if (registeredComponents.find(dependencyId) == registeredComponents.end()) {
                    errors.push_back(formatComponentDependencyError(componentId, dependencyId));
                }
            }
        }
        
        // 如果有错误，生成报告并抛出异常
        if (!errors.empty()) {
            LOG_ERROR("[StateManager] Component dependency validation failed with {} errors:", errors.size());
            for (size_t i = 0; i < errors.size(); ++i) {
                LOG_ERROR("  [{}] {}", i + 1, errors[i].c_str());
            }
            
            std::string errorMsg = "Component dependency validation failed:\n";
            for (const auto& error : errors) {
                errorMsg += "  - " + error + "\n";
            }
            throw ConfigurationError("StateManager", errorMsg);
        }
        
        LOG_INFO("[StateManager] Component dependency validation passed successfully");
    }
    
    /**
     * @brief 格式化组件依赖错误信息
     */
    std::string formatComponentDependencyError(const ComponentId& component, 
                                              const ComponentId& missingDependency) {
        return "Component '" + component.name + 
               "' (vehicle " + std::to_string(component.vehicleId) + 
               ") depends on component '" + missingDependency.name + 
               "' (vehicle " + std::to_string(missingDependency.vehicleId) + 
               ") which is not registered";
    }




    /**
     * @brief 执行优先级感知的拓扑排序
     * @param graph 组件依赖关系图
     * @return 排序后的组件执行顺序
     * @details 使用Kahn算法的变体，在满足依赖约束的前提下考虑组件优先级
     */
    std::vector<ComponentId> performPriorityAwareTopologicalSort(
        const std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>, std::hash<ComponentId>>& graph) {
        
        // 计算每个组件的入度
        std::unordered_map<ComponentId, int, std::hash<ComponentId>> inDegree;
        for (const auto& [component_id, component] : components_) {
            inDegree[component_id] = 0;
        }
        
        // 注意：graph[component] 存储的是 component 依赖的其他组件
        // 所以 component 的入度应该等于它依赖的组件数量
        for (const auto& [component_id, dependencies] : graph) {
            inDegree[component_id] = dependencies.size();
        }
        
        // 使用优先级队列存储入度为0的组件，按优先级排序
        auto priorityComparator = [this](const ComponentId& a, const ComponentId& b) {
            int priorityA = componentPriorities_.count(a) ? componentPriorities_.at(a) : DEFAULT_PRIORITY;
            int priorityB = componentPriorities_.count(b) ? componentPriorities_.at(b) : DEFAULT_PRIORITY;
            
            // 优先级高的组件排在前面（数值大的优先级高）
            if (priorityA != priorityB) {
                return priorityA < priorityB; // 注意：priority_queue是最大堆，所以用<
            }
            
            // 优先级相同时，按组件名字典序排序以确保确定性
            return a.name > b.name;
        };
        
        std::priority_queue<ComponentId, std::vector<ComponentId>, decltype(priorityComparator)> 
            readyQueue(priorityComparator);
        
        // 将所有入度为0的组件加入队列
        for (const auto& [component_id, degree] : inDegree) {
            if (degree == 0) {
                readyQueue.push(component_id);
            }
        }
        
        std::vector<ComponentId> sorted_order;
        
        // Kahn算法主循环
        while (!readyQueue.empty()) {
            ComponentId current = readyQueue.top();
            readyQueue.pop();
            sorted_order.push_back(current);
            
            // 处理所有依赖于当前组件的其他组件
            // 遍历所有组件，找到依赖于current的组件，减少它们的入度
            for (const auto& [component_id, dependencies] : graph) {
                if (dependencies.count(current)) {
                    inDegree[component_id]--;
                    if (inDegree[component_id] == 0) {
                        readyQueue.push(component_id);
                    }
                }
            }
        }
        
        // 检查是否存在循环依赖
        if (sorted_order.size() != components_.size()) {
            // 生成详细的循环依赖诊断信息
            std::string detailed_error = generateCyclicDependencyDiagnostics(graph, inDegree, sorted_order);
            throw DependencyError("StateManager", detailed_error);
        }
        
        executionOrder_ = sorted_order;
        
        // 记录排序过程的详细信息
        logSortingResults(sorted_order, graph);
        
        // 检测并记录优先级冲突
        detectAndLogPriorityConflicts(sorted_order, graph);
        
        LOG_DEBUG("[StateManager] Priority-aware topological sort completed");
        
        return sorted_order;
    }

    /**
     * @brief 记录排序过程的详细结果
     * @param sorted_order 最终的执行顺序
     * @param graph 组件依赖关系图
     */
    void logSortingResults(
        const std::vector<ComponentId>& sorted_order,
        const std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>, std::hash<ComponentId>>& graph) {
        
        LOG_DEBUG("[StateManager] Sorting algorithm results:");
        
        // 1. 记录纯优先级排序结果（如果没有依赖关系的话）
        std::vector<ComponentId> priorityOnlyOrder;
        for (const auto& [componentId, component] : components_) {
            priorityOnlyOrder.push_back(componentId);
        }
        
        // 按优先级排序
        std::sort(priorityOnlyOrder.begin(), priorityOnlyOrder.end(), 
                 [this](const ComponentId& a, const ComponentId& b) {
                     int priorityA = componentPriorities_.count(a) ? componentPriorities_.at(a) : DEFAULT_PRIORITY;
                     int priorityB = componentPriorities_.count(b) ? componentPriorities_.at(b) : DEFAULT_PRIORITY;
                     
                     if (priorityA != priorityB) {
                         return priorityA > priorityB; // 高优先级在前
                     }
                     return a.name < b.name; // 相同优先级按名称排序
                 });
        
        LOG_DEBUG("  Priority-only order (ignoring dependencies):");
        for (size_t i = 0; i < priorityOnlyOrder.size(); ++i) {
            const auto& componentId = priorityOnlyOrder[i];
            int priority = componentPriorities_.count(componentId) ? componentPriorities_.at(componentId) : DEFAULT_PRIORITY;
            LOG_DEBUG("    [{}] {} (priority: {})", i + 1, componentId.name.c_str(), priority);
        }
        
        // 2. 记录依赖关系对排序的影响
        LOG_DEBUG("  Dependency-constrained order (final result):");
        for (size_t i = 0; i < sorted_order.size(); ++i) {
            const auto& componentId = sorted_order[i];
            int priority = componentPriorities_.count(componentId) ? componentPriorities_.at(componentId) : DEFAULT_PRIORITY;
            
            // 计算与纯优先级排序的位置差异
            auto priorityPos = std::find(priorityOnlyOrder.begin(), priorityOnlyOrder.end(), componentId);
            size_t priorityIndex = std::distance(priorityOnlyOrder.begin(), priorityPos);
            
            std::string positionChange;
            if (i == priorityIndex) {
                positionChange = "(same position)";
            } else if (i > priorityIndex) {
                positionChange = "(moved later by " + std::to_string(i - priorityIndex) + ")";
            } else {
                positionChange = "(moved earlier by " + std::to_string(priorityIndex - i) + ")";
            }
            
            LOG_DEBUG("    [{}] {} (priority: {}) {}", 
                     i + 1, componentId.name.c_str(), priority, positionChange.c_str());
        }
        
        // 3. 统计排序算法的影响
        size_t positionChanges = 0;
        for (size_t i = 0; i < sorted_order.size(); ++i) {
            auto priorityPos = std::find(priorityOnlyOrder.begin(), priorityOnlyOrder.end(), sorted_order[i]);
            size_t priorityIndex = std::distance(priorityOnlyOrder.begin(), priorityPos);
            if (i != priorityIndex) {
                positionChanges++;
            }
        }
        
        LOG_DEBUG("  Sorting impact: {}/{} components changed position due to dependency constraints", 
                 positionChanges, sorted_order.size());
    }

    /**
     * @brief 检测并记录优先级冲突
     * @param sorted_order 最终的执行顺序
     * @param graph 组件依赖关系图
     * @details 检测当优先级设置与依赖关系要求冲突时的情况，并记录警告信息
     */
    void detectAndLogPriorityConflicts(
        const std::vector<ComponentId>& sorted_order,
        const std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>, std::hash<ComponentId>>& graph) {
        
        // 创建组件在最终执行顺序中的位置映射
        std::unordered_map<ComponentId, size_t, std::hash<ComponentId>> executionPosition;
        for (size_t i = 0; i < sorted_order.size(); ++i) {
            executionPosition[sorted_order[i]] = i;
        }
        
        std::vector<std::string> conflicts;
        
        // 检查每个依赖关系是否与优先级设置冲突
        for (const auto& [component, dependencies] : graph) {
            int componentPriority = componentPriorities_.count(component) ? componentPriorities_.at(component) : DEFAULT_PRIORITY;
            
            for (const auto& dependency : dependencies) {
                int dependencyPriority = componentPriorities_.count(dependency) ? componentPriorities_.at(dependency) : DEFAULT_PRIORITY;
                
                // 检查优先级冲突：如果依赖组件的优先级低于当前组件，但由于依赖关系必须先执行
                if (componentPriority > dependencyPriority) {
                    // 确认在最终执行顺序中依赖组件确实在当前组件之前
                    if (executionPosition[dependency] < executionPosition[component]) {
                        std::string conflict_msg = "Component '" + component.name + 
                                                 "' (priority " + std::to_string(componentPriority) + 
                                                 ") depends on '" + dependency.name + 
                                                 "' (priority " + std::to_string(dependencyPriority) + 
                                                 "), dependency constraint overrides priority preference";
                        conflicts.push_back(conflict_msg);
                    }
                }
            }
        }
        
        // 记录所有发现的冲突
        if (!conflicts.empty()) {
            LOG_WARN("[StateManager] Detected {} priority conflicts where dependencies override priority preferences:", conflicts.size());
            for (size_t i = 0; i < conflicts.size(); ++i) {
                LOG_WARN("  [{}] {}", i + 1, conflicts[i].c_str());
            }
            LOG_INFO("[StateManager] Dependencies always take precedence over priorities to ensure correct execution order");
        } else {
            LOG_DEBUG("[StateManager] No priority conflicts detected - all priorities are consistent with dependencies");
        }
        
        // 记录最终的执行顺序和优先级信息
        logFinalExecutionOrder(sorted_order);
    }

    /**
     * @brief 记录详细的执行顺序诊断信息
     * @param sorted_order 最终的执行顺序
     */
    void logFinalExecutionOrder(const std::vector<ComponentId>& sorted_order) {
        // 1. 记录优先级统计信息
        logPriorityStatistics();
        
        // 2. 记录依赖关系摘要
        logDependencySummary();
        
        // 3. 记录最终执行顺序
        LOG_INFO("[StateManager] Final execution order determined:");
        LOG_INFO("  ┌─ SIMULATION_LOOP_START");
        
        for (size_t i = 0; i < sorted_order.size(); ++i) {
            const auto& component_id = sorted_order[i];
            int priority = componentPriorities_.count(component_id) ? componentPriorities_.at(component_id) : DEFAULT_PRIORITY;
            
            // 获取依赖信息
            std::string dependencyInfo = formatDependencyInfo(component_id);
            
            std::string prefix = (i == sorted_order.size() - 1) ? "  └─" : "  ├─";
            LOG_INFO("{} [{}] {} (priority: {}) {}", 
                    prefix.c_str(), 
                    i + 1, 
                    component_id.name.c_str(), 
                    priority,
                    dependencyInfo.c_str());
        }
        
        LOG_INFO("  └─ SIMULATION_LOOP_END");
        
        // 4. 记录执行顺序验证结果
        logExecutionOrderValidation(sorted_order);
    }
    
    /**
     * @brief 记录优先级统计信息
     */
    void logPriorityStatistics() {
        if (componentPriorities_.empty()) {
            LOG_DEBUG("[StateManager] No priority information available");
            return;
        }
        
        // 统计优先级分布
        std::map<int, std::vector<std::string>> priorityGroups;
        for (const auto& [componentId, priority] : componentPriorities_) {
            priorityGroups[priority].push_back(componentId.name);
        }
        
        LOG_INFO("[StateManager] Component priority distribution:");
        for (auto it = priorityGroups.rbegin(); it != priorityGroups.rend(); ++it) {
            int priority = it->first;
            const auto& components = it->second;
            
            std::string componentList;
            for (size_t i = 0; i < components.size(); ++i) {
                componentList += components[i];
                if (i < components.size() - 1) componentList += ", ";
            }
            
            std::string priorityLabel = (priority == DEFAULT_PRIORITY) ? " (default)" : "";
            LOG_INFO("  Priority {}{}: {}", priority, priorityLabel.c_str(), componentList.c_str());
        }
    }
    
    /**
     * @brief 记录依赖关系摘要
     */
    void logDependencySummary() {
        LOG_INFO("[StateManager] Component dependency summary:");
        
        if (componentDependencies_.empty()) {
            LOG_INFO("  No component dependencies declared");
            return;
        }
        
        size_t totalDependencies = 0;
        for (const auto& [componentId, dependencies] : componentDependencies_) {
            totalDependencies += dependencies.size();
            
            if (dependencies.empty()) {
                LOG_DEBUG("  {} -> (no dependencies)", componentId.name.c_str());
            } else {
                std::string depList;
                for (auto it = dependencies.begin(); it != dependencies.end(); ++it) {
                    if (it != dependencies.begin()) depList += ", ";
                    depList += it->name;
                }
                LOG_DEBUG("  {} -> depends on: {}", componentId.name.c_str(), depList.c_str());
            }
        }
        
        LOG_INFO("  Total components: {}, Total dependencies: {}", 
                components_.size(), totalDependencies);
    }
    
    /**
     * @brief 格式化组件的依赖信息
     */
    std::string formatDependencyInfo(const ComponentId& componentId) {
        auto it = componentDependencies_.find(componentId);
        if (it == componentDependencies_.end() || it->second.empty()) {
            return "(no deps)";
        }
        
        std::string depInfo = "(deps: ";
        bool first = true;
        for (const auto& dep : it->second) {
            if (!first) depInfo += ", ";
            depInfo += dep.name;
            first = false;
        }
        depInfo += ")";
        
        return depInfo;
    }
    
    /**
     * @brief 记录执行顺序验证结果
     */
    void logExecutionOrderValidation(const std::vector<ComponentId>& sorted_order) {
        LOG_DEBUG("[StateManager] Execution order validation:");
        
        // 验证依赖关系是否得到满足
        std::unordered_map<ComponentId, size_t, std::hash<ComponentId>> executionIndex;
        for (size_t i = 0; i < sorted_order.size(); ++i) {
            executionIndex[sorted_order[i]] = i;
        }
        
        bool allDependenciesSatisfied = true;
        for (const auto& [componentId, dependencies] : componentDependencies_) {
            size_t componentIndex = executionIndex[componentId];
            
            for (const auto& dependency : dependencies) {
                size_t dependencyIndex = executionIndex[dependency];
                if (dependencyIndex >= componentIndex) {
                    LOG_ERROR("  VIOLATION: {} (index {}) should execute after {} (index {})", 
                             componentId.name.c_str(), componentIndex,
                             dependency.name.c_str(), dependencyIndex);
                    allDependenciesSatisfied = false;
                }
            }
        }
        
        if (allDependenciesSatisfied) {
            LOG_INFO("[StateManager] ✓ All dependency constraints satisfied in execution order");
        } else {
            LOG_ERROR("[StateManager] ✗ Dependency constraint violations detected!");
        }
    }

    /**
     * @brief 生成简洁实用的循环依赖诊断信息
     */
    std::string generateCyclicDependencyDiagnostics(
        const std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>, std::hash<ComponentId>>& graph,
        const std::unordered_map<ComponentId, int, std::hash<ComponentId>>& inDegree,
        const std::vector<ComponentId>& sorted_order) {
        
        // 找出参与循环的组件
        std::vector<ComponentId> cyclicComponents;
        for (const auto& [componentId, degree] : inDegree) {
            if (degree > 0) {
                cyclicComponents.push_back(componentId);
            }
        }
        
        LOG_ERROR("[StateManager] ✗ Cyclic dependency detected!");
        LOG_ERROR("[StateManager] Components stuck in cycle ({} components):", cyclicComponents.size());
        
        // 显示每个组件及其依赖关系
        for (const auto& componentId : cyclicComponents) {
            auto it = graph.find(componentId);
            if (it != graph.end() && !it->second.empty()) {
                std::string deps;
                for (auto depIt = it->second.begin(); depIt != it->second.end(); ++depIt) {
                    if (depIt != it->second.begin()) deps += ", ";
                    deps += depIt->name;
                }
                LOG_ERROR("  {} → depends on: {}", componentId.name.c_str(), deps.c_str());
            }
        }
        
        // 生成错误消息
        std::string errorMsg = "Cyclic dependency detected. Components involved: ";
        for (size_t i = 0; i < cyclicComponents.size(); ++i) {
            errorMsg += cyclicComponents[i].name;
            if (i < cyclicComponents.size() - 1) errorMsg += ", ";
        }
        errorMsg += ". Check the dependency relationships above and remove circular references.";
        
        return errorMsg;
    }

    std::unordered_map<ComponentId, ComponentBase*, std::hash<ComponentId>> components_;
    std::unordered_map<StateId, std::any, std::hash<StateId>> states_;
    std::vector<ComponentId> executionOrder_;
    std::unordered_map<ComponentId, std::unordered_set<ComponentId, std::hash<ComponentId>>, std::hash<ComponentId>> componentDependencies_;
    std::unordered_map<ComponentId, int, std::hash<ComponentId>> componentPriorities_;
    static constexpr int DEFAULT_PRIORITY = 500;
    bool needsRevalidation_{true};
};

} // namespace gnc