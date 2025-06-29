#pragma once

#include "../../../core/component_base.hpp"
#include "../../../core/component_registrar.hpp"
#include "../simple_logger.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <memory>
#include <optional>

namespace gnc::components::utility {

/**
 * @brief 流程控制器组件
 * 
 * @details 流程控制器是一个通用的状态机实现，用于管理复杂的流程控制逻辑。
 * 它允许用户定义状态、转换条件和动作，而不需要编写复杂的条件分支代码。
 * 
 * 主要特点：
 * 1. 状态定义：支持自定义状态和初始状态
 * 2. 转换条件：基于条件函数的状态转换
 * 3. 状态动作：进入状态、离开状态和状态内动作
 * 4. 事件驱动：支持外部事件触发状态转换
 * 5. 历史记录：记录状态转换历史
 * 
 * 使用场景：
 * - 飞行阶段管理（发射、上升、巡航、下降、着陆等）
 * - 制导律切换（不同飞行阶段使用不同的制导律）
 * - 导航模式切换（根据传感器可用性切换不同的导航模式）
 * - 控制模式切换（姿态控制、速度控制、位置控制等）
 * - 故障处理流程（检测、诊断、恢复等）
 */
class FlowController : public states::ComponentBase {
public:
    /**
     * @brief 状态类型（使用字符串表示状态名称）
     */
    using StateType = std::string;
    
    /**
     * @brief 条件函数类型，返回是否满足转换条件
     */
    using ConditionFunc = std::function<bool()>;
    
    /**
     * @brief 动作函数类型，在状态转换或状态内执行
     */
    using ActionFunc = std::function<void()>;
    
    /**
     * @brief 状态转换定义
     */
    struct Transition {
        StateType from_state;       ///< 源状态
        StateType to_state;         ///< 目标状态
        ConditionFunc condition;    ///< 转换条件
        ActionFunc action;          ///< 转换动作（可选）
        std::string description;    ///< 转换描述（可选，用于日志和调试）
    };
    
    /**
     * @brief 状态定义
     */
    struct State {
        StateType name;                 ///< 状态名称
        ActionFunc entry_action;        ///< 进入状态时执行的动作（可选）
        ActionFunc exit_action;         ///< 离开状态时执行的动作（可选）
        ActionFunc update_action;       ///< 状态内每次更新时执行的动作（可选）
        std::string description;        ///< 状态描述（可选，用于日志和调试）
    };

    /**
     * @brief 构造函数
     * 
     * @param id 飞行器ID
     * @param name 组件名称
     * @param initial_state 初始状态名称
     */
    FlowController(states::VehicleId id, const std::string& name, const StateType& initial_state)
        : states::ComponentBase(id, name), current_state_(initial_state), initial_state_(initial_state) {
        
        // 声明输出状态
        declareOutput<StateType>("current_state");  // 当前状态
        declareOutput<StateType>("previous_state"); // 前一个状态
        declareOutput<double>("time_in_state");     // 在当前状态停留的时间
        declareOutput<bool>("state_changed");       // 本次更新是否发生了状态变化
    }

    /**
     * @brief 获取组件类型
     */
    std::string getComponentType() const override {
        return "FlowController";
    }

    /**
     * @brief 添加状态
     * 
     * @param state 状态定义
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& addState(const State& state) {
        states_[state.name] = state;
        return *this;
    }

    /**
     * @brief 添加状态（简化版本）
     * 
     * @param name 状态名称
     * @param description 状态描述
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& addState(const StateType& name, const std::string& description = "") {
        State state;
        state.name = name;
        state.description = description;
        return addState(state);
    }

    /**
     * @brief 添加转换
     * 
     * @param transition 转换定义
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& addTransition(const Transition& transition) {
        transitions_.push_back(transition);
        return *this;
    }

    /**
     * @brief 添加转换（简化版本）
     * 
     * @param from_state 源状态
     * @param to_state 目标状态
     * @param condition 转换条件
     * @param description 转换描述
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& addTransition(
        const StateType& from_state,
        const StateType& to_state,
        ConditionFunc condition,
        const std::string& description = ""
    ) {
        Transition transition;
        transition.from_state = from_state;
        transition.to_state = to_state;
        transition.condition = condition;
        transition.description = description;
        return addTransition(transition);
    }

    /**
     * @brief 设置状态进入动作
     * 
     * @param state 状态名称
     * @param action 进入动作
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& setEntryAction(const StateType& state, ActionFunc action) {
        if (states_.find(state) != states_.end()) {
            states_[state].entry_action = action;
        }
        return *this;
    }

    /**
     * @brief 设置状态离开动作
     * 
     * @param state 状态名称
     * @param action 离开动作
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& setExitAction(const StateType& state, ActionFunc action) {
        if (states_.find(state) != states_.end()) {
            states_[state].exit_action = action;
        }
        return *this;
    }

    /**
     * @brief 设置状态更新动作
     * 
     * @param state 状态名称
     * @param action 更新动作
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& setUpdateAction(const StateType& state, ActionFunc action) {
        if (states_.find(state) != states_.end()) {
            states_[state].update_action = action;
        }
        return *this;
    }

    /**
     * @brief 强制切换到指定状态
     * 
     * @param state 目标状态
     * @return bool 切换是否成功
     */
    bool forceTransition(const StateType& state) {
        if (states_.find(state) == states_.end()) {
            LOG_COMPONENT_ERROR("Cannot transition to unknown state: {}", state.c_str());
            return false;
        }

        // 执行当前状态的离开动作
        if (states_.find(current_state_) != states_.end() && states_[current_state_].exit_action) {
            states_[current_state_].exit_action();
        }

        // 记录前一个状态
        previous_state_ = current_state_;
        
        // 更新当前状态
        current_state_ = state;
        time_in_state_ = 0.0;
        state_changed_ = true;
        
        // 记录状态转换历史
        state_history_.push_back({previous_state_, current_state_, "Forced transition"});
        if (state_history_.size() > max_history_size_) {
            state_history_.erase(state_history_.begin());
        }
        
        // 执行新状态的进入动作
        if (states_[current_state_].entry_action) {
            states_[current_state_].entry_action();
        }
        
        LOG_COMPONENT_INFO("Forced state transition: {} -> {}", previous_state_.c_str(), current_state_.c_str());
        return true;
    }

    /**
     * @brief 触发事件，检查是否有基于此事件的转换
     * 
     * @param event_name 事件名称
     * @return bool 是否触发了状态转换
     */
    bool triggerEvent(const std::string& event_name) {
        for (const auto& transition : transitions_) {
            if (transition.from_state == current_state_ && 
                event_transitions_.find(event_name) != event_transitions_.end() &&
                event_transitions_[event_name].find(current_state_) != event_transitions_[event_name].end() &&
                event_transitions_[event_name][current_state_] == transition.to_state) {
                
                // 执行转换
                previous_state_ = current_state_;
                
                // 执行当前状态的离开动作
                if (states_[current_state_].exit_action) {
                    states_[current_state_].exit_action();
                }
                
                // 执行转换动作
                if (transition.action) {
                    transition.action();
                }
                
                // 更新当前状态
                current_state_ = transition.to_state;
                time_in_state_ = 0.0;
                state_changed_ = true;
                
                // 记录状态转换历史
                state_history_.push_back({previous_state_, current_state_, "Event: " + event_name});
                if (state_history_.size() > max_history_size_) {
                    state_history_.erase(state_history_.begin());
                }
                
                // 执行新状态的进入动作
                if (states_[current_state_].entry_action) {
                    states_[current_state_].entry_action();
                }
                
                LOG_COMPONENT_INFO("Event triggered state transition: {} -> {} (Event: {})", 
                          previous_state_.c_str(), current_state_.c_str(), event_name.c_str());
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 添加事件转换
     * 
     * @param event_name 事件名称
     * @param from_state 源状态
     * @param to_state 目标状态
     * @return FlowController& 流程控制器引用（链式调用）
     */
    FlowController& addEventTransition(
        const std::string& event_name,
        const StateType& from_state,
        const StateType& to_state
    ) {
        event_transitions_[event_name][from_state] = to_state;
        return *this;
    }

    /**
     * @brief 获取当前状态
     * 
     * @return const StateType& 当前状态
     */
    const StateType& getCurrentState() const {
        return current_state_;
    }

    /**
     * @brief 获取前一个状态
     * 
     * @return const StateType& 前一个状态
     */
    const StateType& getPreviousState() const {
        return previous_state_;
    }

    /**
     * @brief 获取在当前状态的停留时间
     * 
     * @return double 停留时间（秒）
     */
    double getTimeInState() const {
        return time_in_state_;
    }

    /**
     * @brief 检查本次更新是否发生了状态变化
     * 
     * @return bool 是否发生状态变化
     */
    bool hasStateChanged() const {
        return state_changed_;
    }

    /**
     * @brief 重置流程控制器到初始状态
     */
    void reset() {
        // 如果当前不在初始状态，执行离开动作
        if (current_state_ != initial_state_ && 
            states_.find(current_state_) != states_.end() && 
            states_[current_state_].exit_action) {
            states_[current_state_].exit_action();
        }
        
        previous_state_ = current_state_;
        current_state_ = initial_state_;
        time_in_state_ = 0.0;
        state_changed_ = true;
        state_history_.clear();
        
        // 执行初始状态的进入动作
        if (states_.find(initial_state_) != states_.end() && 
            states_[initial_state_].entry_action) {
            states_[initial_state_].entry_action();
        }
        
        LOG_COMPONENT_INFO("Flow controller reset to initial state: {}", initial_state_.c_str());
    }

    /**
     * @brief 获取状态历史记录
     * 
     * @return const std::vector<StateTransitionRecord>& 状态转换历史记录
     */
    struct StateTransitionRecord {
        StateType from_state;     ///< 源状态
        StateType to_state;       ///< 目标状态
        std::string reason;       ///< 转换原因
    };
    
    const std::vector<StateTransitionRecord>& getStateHistory() const {
        return state_history_;
    }

    /**
     * @brief 设置最大历史记录大小
     * 
     * @param size 最大记录数量
     */
    void setMaxHistorySize(size_t size) {
        max_history_size_ = size;
        if (state_history_.size() > max_history_size_) {
            state_history_.erase(state_history_.begin(), 
                               state_history_.begin() + (state_history_.size() - max_history_size_));
        }
    }

protected:
    /**
     * @brief 组件更新实现
     */
    void updateImpl() override {
        // 重置状态变化标志
        state_changed_ = false;
        
        // 增加当前状态停留时间
        // TODO 实现全局的时间组件
        time_in_state_ += 0.01; // 假设更新频率为100Hz，可以通过配置调整
        
        // 检查所有可能的转换
        for (const auto& transition : transitions_) {
            if (transition.from_state == current_state_ && transition.condition && transition.condition()) {
                // 执行当前状态的离开动作
                if (states_.find(current_state_) != states_.end() && states_[current_state_].exit_action) {
                    states_[current_state_].exit_action();
                }
                
                // 记录前一个状态
                previous_state_ = current_state_;
                
                // 执行转换动作
                if (transition.action) {
                    transition.action();
                }
                
                // 更新当前状态
                current_state_ = transition.to_state;
                time_in_state_ = 0.0;
                state_changed_ = true;
                
                // 记录状态转换历史
                state_history_.push_back({previous_state_, current_state_, 
                                       transition.description.empty() ? "Condition met" : transition.description});
                if (state_history_.size() > max_history_size_) {
                    state_history_.erase(state_history_.begin());
                }
                
                // 执行新状态的进入动作
                if (states_.find(current_state_) != states_.end() && states_[current_state_].entry_action) {
                    states_[current_state_].entry_action();
                }
                
                LOG_COMPONENT_INFO("State transition: {} -> {} ({})", 
                          previous_state_.c_str(), current_state_.c_str(), 
                          transition.description.empty() ? "Condition met" : transition.description.c_str());
                
                // 只执行第一个满足条件的转换
                break;
            }
        }
        
        // 执行当前状态的更新动作
        if (states_.find(current_state_) != states_.end() && states_[current_state_].update_action) {
            states_[current_state_].update_action();
        }
        
        // 作为工具组件，不会使用状态管理器注册，不允许使用状态管理器setState进行状态更新
    }

    /**
     * @brief 初始化组件
     */
    void initialize() override {
        // 检查状态和转换的有效性
        validateStatesAndTransitions();
        
        // 确保初始状态存在
        if (states_.find(initial_state_) == states_.end()) {
            LOG_COMPONENT_ERROR("Initial state '{}' not defined", initial_state_.c_str());
            // 添加一个默认的初始状态
            addState(initial_state_, "Default initial state");
        }
        
        // 执行初始状态的进入动作
        if (states_[initial_state_].entry_action) {
            states_[initial_state_].entry_action();
        }
        
        LOG_COMPONENT_INFO("Flow controller initialized with state: {}", initial_state_.c_str());
    }

private:
    /**
     * @brief 验证状态和转换的有效性
     */
    void validateStatesAndTransitions() {
        // 检查所有转换的状态是否已定义
        for (const auto& transition : transitions_) {
            if (states_.find(transition.from_state) == states_.end()) {
                LOG_COMPONENT_WARN("Transition from undefined state: {}", transition.from_state.c_str());
                // 自动添加缺失的状态
                addState(transition.from_state, "Auto-generated state");
            }
            
            if (states_.find(transition.to_state) == states_.end()) {
                LOG_COMPONENT_WARN("Transition to undefined state: {}", transition.to_state.c_str());
                // 自动添加缺失的状态
                addState(transition.to_state, "Auto-generated state");
            }
        }
        
        // 检查事件转换
        for (const auto& event_entry : event_transitions_) {
            for (const auto& state_entry : event_entry.second) {
                if (states_.find(state_entry.first) == states_.end()) {
                    LOG_COMPONENT_WARN("Event transition from undefined state: {}", state_entry.first.c_str());
                    // 自动添加缺失的状态
                    addState(state_entry.first, "Auto-generated state");
                }
                
                if (states_.find(state_entry.second) == states_.end()) {
                    LOG_COMPONENT_WARN("Event transition to undefined state: {}", state_entry.second.c_str());
                    // 自动添加缺失的状态
                    addState(state_entry.second, "Auto-generated state");
                }
            }
        }
    }

private:
    StateType current_state_;     ///< 当前状态
    StateType previous_state_;    ///< 前一个状态
    StateType initial_state_;     ///< 初始状态
    double time_in_state_ = 0.0;  ///< 在当前状态停留的时间（秒）
    bool state_changed_ = false;  ///< 本次更新是否发生了状态变化
    
    std::unordered_map<StateType, State> states_;  ///< 状态定义映射
    std::vector<Transition> transitions_;           ///< 转换定义列表
    
    // 事件转换映射：event_name -> {from_state -> to_state}
    std::unordered_map<std::string, std::unordered_map<StateType, StateType>> event_transitions_;
    
    // 状态转换历史记录
    std::vector<StateTransitionRecord> state_history_;
    size_t max_history_size_ = 100;  ///< 最大历史记录大小
};
// 作为工具，不进行注册，由使用的组件手动注册
} // namespace gnc::components::utility