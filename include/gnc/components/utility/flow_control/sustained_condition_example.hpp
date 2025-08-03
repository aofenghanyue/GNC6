#pragma once

#include "../../../core/component_base.hpp"
#include "../../../core/component_registrar.hpp"
#include "flow_controller.hpp"
#include <memory>

namespace gnc::components::utility {

/**
 * @brief 持续条件检查示例组件
 * 
 * @details 展示FlowController新功能的使用：
 * 1. 链式API使用方法
 * 2. 持续条件检查（防误切换）
 * 3. 基于时间和周期的条件
 * 4. 重置策略配置
 */
class SustainedConditionExample : public states::ComponentBase {
public:
    SustainedConditionExample(states::VehicleId id, const std::string& instanceName = "")
        : states::ComponentBase(id, "SustainedConditionExample", instanceName), cycle_count_(0) {
        
        // 简化的组件级依赖声明
        declareInput<void>(ComponentId{id, "Navigation"});
        declareInput<void>(ComponentId{id, "Control"});
        
        // 声明输出状态
        declareOutput<std::string>("current_phase");
        declareOutput<std::string>("transition_reason");
        declareOutput<int>("demo_counter");
    }

    std::string getComponentType() const override {
        return "SustainedConditionExample";
    }

protected:
    void initialize() override {
        ComponentBase::initialize();
        initFlowController();
    }
    
    void updateImpl() override {
        cycle_count_++;
        
        // 模拟传感器数据
        double altitude = cycle_count_ * 5.0;
        double speed = cycle_count_ * 2.0;
        double throttle = cycle_count_ > 10 ? 0.9 : 0.3;
        
        // 更新流程控制器
        flow_controller_->update();
        
        // 输出状态
        setState("current_phase", flow_controller_->getCurrentState());
        setState("transition_reason", flow_controller_->getLastTransitionReason());
        setState("demo_counter", cycle_count_);
        
        if (flow_controller_->hasStateChanged()) {
            LOG_COMPONENT_INFO("State changed: {}", flow_controller_->getLastTransitionReason().c_str());
        }
    }

private:
    void initFlowController() {
        flow_controller_ = std::make_unique<FlowController>(
            getVehicleId(), getName() + "_FlowController", "ground", getStateAccess());
        
        // 添加状态
        flow_controller_->addState("ground", "On ground")
                        .addState("preparation", "Preparing for takeoff")
                        .addState("takeoff", "Taking off")
                        .addState("climb", "Climbing")
                        .addState("cruise", "Cruising");
        
        // === 使用新的链式API配置转换 ===
        
        // 示例1：基于周期的持续条件
        // 地面 -> 准备：油门>0.8且持续5个周期
        flow_controller_->addTransition("ground", "preparation")
            ->withCondition([this]() { 
                return cycle_count_ > 10;  // 模拟油门条件
            })
            ->sustainedFor(5)  // 需要连续5个周期满足条件
            ->withDescription("Throttle high for 5 cycles");
        
        // 示例2：基于时间的持续条件
        // 准备 -> 起飞：速度>50且持续1秒
        flow_controller_->addTransition("preparation", "takeoff")
            ->withCondition([this]() { 
                return cycle_count_ > 20;  // 模拟速度条件
            })
            ->sustainedForSeconds(1.0)  // 需要持续1秒
            ->withDescription("Takeoff speed sustained for 1s");
        
        // 示例3：带重置策略的条件
        // 起飞 -> 爬升：高度>100，条件中断时重新计数
        flow_controller_->addTransition("takeoff", "climb")
            ->withCondition([this]() {
                // 模拟一个会偶尔中断的条件
                return cycle_count_ > 30 && (cycle_count_ % 7 != 0);
            })
            ->sustainedFor(3)
            ->resetOnFalse(true)  // 条件不满足时立即重置计数
            ->withDescription("Stable climb conditions");
        
        // 示例4：不重置计数的条件（允许短暂中断）
        // 爬升 -> 巡航：高度>1000，允许条件短暂不满足
        flow_controller_->addTransition("climb", "cruise")
            ->withCondition([this]() {
                return cycle_count_ > 40;
            })
            ->sustainedFor(5)
            ->resetOnFalse(false)  // 条件短暂不满足时保持计数
            ->withDescription("Cruise altitude reached");
        
        // 示例5：传统API仍然可用（立即转换）
        flow_controller_->addTransition("cruise", "ground",
            [this]() { return cycle_count_ > 60; },
            "Simulation reset");
        
        // 设置状态动作
        flow_controller_->setEntryAction("preparation", [this]() {
            LOG_COMPONENT_INFO("=== Starting preparation phase ===");
        });
        
        flow_controller_->setEntryAction("takeoff", [this]() {
            LOG_COMPONENT_INFO("=== Takeoff initiated ===");
        });
        
        flow_controller_->setEntryAction("climb", [this]() {
            LOG_COMPONENT_INFO("=== Climbing to altitude ===");
        });
        
        flow_controller_->setEntryAction("cruise", [this]() {
            LOG_COMPONENT_INFO("=== Cruise phase reached ===");
        });
    }

private:
    std::unique_ptr<FlowController> flow_controller_;
    int cycle_count_;
};

// 注册示例组件
static gnc::ComponentRegistrar<SustainedConditionExample> sustained_example_registrar("SustainedConditionExample");

} // namespace gnc::components::utility