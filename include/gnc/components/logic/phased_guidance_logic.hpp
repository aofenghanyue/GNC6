#pragma once

#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include "../utility/flow_control/flow_controller.hpp"
#include "../utility/simple_logger.hpp"
#include <memory>
#include <vector>

namespace gnc::components {

/**
 * @brief 分阶段制导律组件
 * 
 * @details 这是一个使用流程控制器管理不同制导阶段的组件。
 * 包含两个制导阶段：
 * - 初始制导阶段：运行前5个周期
 * - 主制导阶段：第6个周期开始
 * 
 * 每个阶段使用不同的制导律参数和导引量计算方法。
 */
class PhasedGuidanceLogic : public states::ComponentBase {
public:
    /**
     * @brief 制导阶段枚举
     */
    enum class GuidancePhase {
        INITIAL,    ///< 初始制导阶段
        MAIN        ///< 主制导阶段
    };
    
    /**
     * @brief 构造函数
     * 
     * @param id 飞行器ID
     * @param instanceName 组件实例名称（可选）
     */
    PhasedGuidanceLogic(states::VehicleId id, const std::string& instanceName = "")
        : states::ComponentBase(id, "Guidance", instanceName), 
          cycle_count_(0) {
        
        // 声明输入状态
        declareInput<Vector3d>("nav_pva", {{id, "Navigation"}, "pva_estimate"});
        
        // 声明输出状态
        declareOutput<std::string>("current_phase");        // 当前制导阶段名称
        declareOutput<int>("phase_id");                     // 当前制导阶段ID
        declareOutput<bool>("phase_changed");               // 制导阶段是否变化
        declareOutput<double>("time_in_phase");             // 在当前阶段的时间
        declareOutput<std::vector<double>>("guidance_command"); // 导引量输出
        declareOutput<double>("desired_throttle_level");    // 油门指令
        
        // 注意：不在构造函数中初始化FlowController，而是在initialize()中进行
    }

    /**
     * @brief 获取组件类型
     */
    std::string getComponentType() const override {
        return "Guidance";
    }

    /**
     * @brief 获取当前制导阶段
     */
    GuidancePhase getCurrentPhase() const {
        const std::string& phase = flow_controller_->getCurrentState();
        
        if (phase == "initial") return GuidancePhase::INITIAL;
        if (phase == "main") return GuidancePhase::MAIN;
        
        // 默认返回初始阶段
        return GuidancePhase::INITIAL;
    }

    /**
     * @brief 强制切换到指定制导阶段
     * 
     * @param phase 目标制导阶段
     * @return bool 切换是否成功
     */
    bool forcePhase(GuidancePhase phase) {
        std::string phase_name;
        
        switch (phase) {
            case GuidancePhase::INITIAL: phase_name = "initial"; break;
            case GuidancePhase::MAIN: phase_name = "main"; break;
        }
        
        return flow_controller_->forceTransition(phase_name);
    }

protected:
    /**
     * @brief 初始化组件
     * 
     * @details 在StateManager注册组件后调用，此时可以访问状态管理器
     */
    void initialize() override {
        // 调用基类的初始化
        ComponentBase::initialize();
        
        // 现在可以安全地初始化FlowController，因为组件已经被注册到StateManager
        initFlowController();
        
        LOG_COMPONENT_INFO("PhasedGuidanceLogic initialized with FlowController");
    }

    /**
     * @brief 组件更新实现
     */
    void updateImpl() override {
        // 增加周期计数
        cycle_count_++;
        
        // 更新流程控制器
        flow_controller_->update();
        
        // 获取当前制导阶段信息
        std::string current_phase = flow_controller_->getCurrentState();
        bool phase_changed = flow_controller_->hasStateChanged();
        double time_in_phase = flow_controller_->getTimeInState();
        
        // 根据当前阶段计算制导律
        std::vector<double> guidance_command = calculateGuidanceCommand(current_phase);
        double throttle_command = calculateThrottleCommand(current_phase);
        
        // 更新输出状态
        setState("current_phase", current_phase);
        setState("phase_id", static_cast<int>(getCurrentPhase()));
        setState("phase_changed", phase_changed);
        setState("time_in_phase", time_in_phase);
        setState("guidance_command", guidance_command);
        setState("desired_throttle_level", throttle_command);
        
        // 记录制导阶段变化
        if (phase_changed) {
            LOG_COMPONENT_INFO("Guidance phase changed to: {} (cycle: {})",
                      current_phase.c_str(), cycle_count_);
        }
        
        // 记录制导输出
        LOG_COMPONENT_DEBUG("Phase: {}, Guidance: [{:.3f}, {:.3f}, {:.3f}], Throttle: {:.3f}, Time in phase: {:.2f}s",
                   current_phase.c_str(), guidance_command[0], guidance_command[1], guidance_command[2], throttle_command, time_in_phase);
    }

private:
    /**
     * @brief 初始化流程控制器
     */
    void initFlowController() {
        // 创建流程控制器，传入状态访问接口
        flow_controller_ = std::make_unique<utility::FlowController>(
            getVehicleId(), getName() + "_FlowController", "initial", getStateAccess());
        
        // 添加状态
        flow_controller_->addState("initial", "Initial guidance phase")
                        .addState("main", "Main guidance phase");
        
        // 添加转换条件：运行5个周期后切换到主制导阶段
        flow_controller_->addTransition("initial", "main", [this]() {
            return cycle_count_ >= 5;
        }, "Switch to main guidance after 5 cycles");
        
        // 设置状态动作
        flow_controller_->setEntryAction("initial", [this]() {
            LOG_COMPONENT_INFO("Entered initial guidance phase");
        });
        
        flow_controller_->setEntryAction("main", [this]() {
            LOG_COMPONENT_INFO("Entered main guidance phase");
        });
        
        flow_controller_->setUpdateAction("initial", [this]() {
            LOG_COMPONENT_DEBUG("Running initial guidance (cycle: {})", cycle_count_);
        });
        
        flow_controller_->setUpdateAction("main", [this]() {
            LOG_COMPONENT_DEBUG("Running main guidance (cycle: {})", cycle_count_);
        });
    }
    
    /**
     * @brief 计算制导指令
     * 
     * @param phase 当前制导阶段
     * @return std::vector<double> 制导指令 [x, y, z]
     */
    std::vector<double> calculateGuidanceCommand(const std::string& phase) {
        std::vector<double> command(3, 0.0);
        
        if (phase == "initial") {
            // 初始制导阶段：使用保守的制导参数
            command[0] = 1.0;  // X方向导引量
            command[1] = 0.5;  // Y方向导引量
            command[2] = 0.2;  // Z方向导引量
        } else if (phase == "main") {
            // 主制导阶段：使用更积极的制导参数
            command[0] = 2.5;  // X方向导引量
            command[1] = 1.8;  // Y方向导引量
            command[2] = 1.2;  // Z方向导引量
        }
        
        return command;
    }
    
    /**
     * @brief 计算油门指令
     * 
     * @param phase 当前制导阶段
     * @return double 油门指令
     */
    double calculateThrottleCommand(const std::string& phase) {
        if (phase == "initial") {
            // 初始制导阶段：较低的油门
            return 0.6;
        } else if (phase == "main") {
            // 主制导阶段：较高的油门
            return 0.85;
        }
        
        return 0.5; // 默认值
    }

private:
    std::unique_ptr<utility::FlowController> flow_controller_;  ///< 流程控制器
    int cycle_count_;                                           ///< 周期计数器
};

// 注册分阶段制导律组件到组件工厂
static gnc::ComponentRegistrar<PhasedGuidanceLogic> phased_guidance_logic_registrar("PhasedGuidanceLogic");

}