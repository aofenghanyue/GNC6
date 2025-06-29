// 这是单独为需要流程控制的状态创建的组件，一般的流程管理不用这种最复杂的方式，而是直接在需用组件内部创建流程管理
#pragma once

#include "flow_controller.hpp"
#include "../../../core/component_base.hpp"
#include "../../../core/component_registrar.hpp"
#include <memory>

namespace gnc::components::utility {

/**
 * @brief 飞行阶段管理器组件
 * 
 * @details 这是一个示例组件，展示如何使用流程控制器管理飞行阶段。
 * 飞行阶段包括：地面、起飞、爬升、巡航、下降、着陆。
 * 该组件根据飞行状态自动切换飞行阶段，并提供相应的飞行阶段信息。
 */
class FlightPhaseManager : public states::ComponentBase {
public:
    /**
     * @brief 飞行阶段枚举
     */
    enum class FlightPhase {
        GROUND,    ///< 地面
        TAKEOFF,   ///< 起飞
        CLIMB,     ///< 爬升
        CRUISE,    ///< 巡航
        DESCENT,   ///< 下降
        LANDING    ///< 着陆
    };
    
    /**
     * @brief 构造函数
     * 
     * @param id 飞行器ID
     * @param instanceName 组件实例名称（可选）
     */
    FlightPhaseManager(states::VehicleId id, const std::string& instanceName = "")
        : states::ComponentBase(id, "FlightPhaseManager", instanceName) {
        
        // 声明输入状态
        declareInput<double>("altitude", {{id, "Navigation"}, "altitude"});
        declareInput<double>("airspeed", {{id, "Navigation"}, "airspeed"});
        declareInput<bool>("on_ground", {{id, "Navigation"}, "on_ground"});
        declareInput<double>("distance_to_destination", {{id, "Navigation"}, "distance_to_destination"}, false);
        
        // 声明输出状态
        declareOutput<std::string>("current_phase");  // 当前飞行阶段名称
        declareOutput<int>("phase_id");               // 当前飞行阶段ID
        declareOutput<bool>("phase_changed");         // 飞行阶段是否变化
        declareOutput<double>("time_in_phase");       // 在当前阶段的时间
        
        // 创建并配置流程控制器
        initFlowController();
    }

    /**
     * @brief 获取组件类型
     */
    std::string getComponentType() const override {
        return "FlightPhaseManager";
    }

    /**
     * @brief 获取当前飞行阶段
     */
    FlightPhase getCurrentPhase() const {
        const std::string& phase = flow_controller_->getCurrentState();
        
        if (phase == "ground") return FlightPhase::GROUND;
        if (phase == "takeoff") return FlightPhase::TAKEOFF;
        if (phase == "climb") return FlightPhase::CLIMB;
        if (phase == "cruise") return FlightPhase::CRUISE;
        if (phase == "descent") return FlightPhase::DESCENT;
        if (phase == "landing") return FlightPhase::LANDING;
        
        // 默认返回地面阶段
        return FlightPhase::GROUND;
    }

    /**
     * @brief 强制切换到指定飞行阶段
     * 
     * @param phase 目标飞行阶段
     * @return bool 切换是否成功
     */
    bool forcePhase(FlightPhase phase) {
        std::string phase_name;
        
        switch (phase) {
            case FlightPhase::GROUND: phase_name = "ground"; break;
            case FlightPhase::TAKEOFF: phase_name = "takeoff"; break;
            case FlightPhase::CLIMB: phase_name = "climb"; break;
            case FlightPhase::CRUISE: phase_name = "cruise"; break;
            case FlightPhase::DESCENT: phase_name = "descent"; break;
            case FlightPhase::LANDING: phase_name = "landing"; break;
        }
        
        return flow_controller_->forceTransition(phase_name);
    }

    /**
     * @brief 触发紧急事件
     * 
     * @param event_name 事件名称（如 "emergency_landing"）
     * @return bool 是否触发了阶段切换
     */
    bool triggerEvent(const std::string& event_name) {
        return flow_controller_->triggerEvent(event_name);
    }

protected:
    /**
     * @brief 组件更新实现
     */
    void updateImpl() override {
        // 更新流程控制器
        flow_controller_->update();
        
        // 获取当前飞行阶段信息
        std::string current_phase = flow_controller_->getCurrentState();
        bool phase_changed = flow_controller_->hasStateChanged();
        double time_in_phase = flow_controller_->getTimeInState();
        
        // 更新输出状态
        setState("current_phase", current_phase);
        setState("phase_id", static_cast<int>(getCurrentPhase()));
        setState("phase_changed", phase_changed);
        setState("time_in_phase", time_in_phase);
        
        // 记录飞行阶段变化
        if (phase_changed) {
            LOG_COMPONENT_INFO("Flight phase changed to: {} (after {:.2f} seconds in previous phase)",
                      current_phase, time_in_phase);
        }
    }

private:
    /**
     * @brief 初始化流程控制器
     */
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
        // 地面 -> 起飞（空速 > 50）
        flow_controller_->addTransition("ground", "takeoff", [this]() {
            return getState<double>("airspeed") > 50.0;
        }, "Takeoff speed reached");
        
        // 起飞 -> 爬升（高度 > 100）
        flow_controller_->addTransition("takeoff", "climb", [this]() {
            return getState<double>("altitude") > 100.0;
        }, "Aircraft has left the ground");
        
        // 爬升 -> 巡航（高度 > 巡航高度 - 100）
        flow_controller_->addTransition("climb", "cruise", [this]() {
            // 假设巡航高度为10000米
            const double cruise_altitude = 10000.0;
            return getState<double>("altitude") > cruise_altitude - 100.0;
        }, "Cruise altitude reached");
        
        // 巡航 -> 下降（到目的地距离 < 100公里）
        flow_controller_->addTransition("cruise", "descent", [this]() {
            try {
                return getState<double>("distance_to_destination") < 100000.0;
            } catch (const std::exception&) {
                // 如果没有距离信息，则不触发转换
                return false;
            }
        }, "Approaching destination");
        
        // 下降 -> 着陆（高度 < 1000）
        flow_controller_->addTransition("descent", "landing", [this]() {
            return getState<double>("altitude") < 1000.0;
        }, "Approaching runway");
        
        // 着陆 -> 地面（接地）
        flow_controller_->addTransition("landing", "ground", [this]() {
            return getState<bool>("on_ground");
        }, "Aircraft has touched down");
        
        // 添加事件转换
        flow_controller_->addEventTransition("emergency_landing", "cruise", "descent")
                        .addEventTransition("emergency_landing", "climb", "descent")
                        .addEventTransition("abort_takeoff", "takeoff", "ground");
        
        // 设置状态动作
        flow_controller_->setEntryAction("ground", [this]() {
            LOG_COMPONENT_INFO("Aircraft is now on ground");
        });
        
        flow_controller_->setEntryAction("takeoff", [this]() {
            LOG_COMPONENT_INFO("Aircraft is now taking off");
        });
        
        flow_controller_->setEntryAction("cruise", [this]() {
            LOG_COMPONENT_INFO("Aircraft has reached cruise altitude");
        });
        
        flow_controller_->setEntryAction("descent", [this]() {
            LOG_COMPONENT_INFO("Aircraft has started descent");
        });
    }

private:
    std::unique_ptr<FlowController> flow_controller_;  ///< 流程控制器
};

// 注册飞行阶段管理器组件到组件工厂
static gnc::ComponentRegistrar<FlightPhaseManager> flight_phase_manager_registrar("FlightPhaseManager");

} // namespace gnc::components::utility