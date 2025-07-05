/**
 * @file simple_guidance_with_transform.hpp
 * @brief 使用坐标转换系统的简单制导组件示例
 * 
 * 这个示例展示了如何使用超简化的坐标转换接口
 */

#pragma once

#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include "../../coordination/coordination.hpp"
#include "../utility/simple_logger.hpp"

namespace gnc {
namespace components {

/**
 * @brief 使用坐标转换的简单制导组件
 * 
 * 展示一行代码完成坐标转换的便捷性
 */
class SimpleGuidanceWithTransform : public states::ComponentBase {
public:
    SimpleGuidanceWithTransform(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "SimpleGuidance", instanceName) {
        
        // 声明输入
        declareInput<Vector3d>("position_inertial", {{id, "Navigation"}, "position_estimate"});
        declareInput<Vector3d>("velocity_inertial", {{id, "Navigation"}, "velocity_estimate"});
        declareInput<Vector3d>("target_position", {{id, "TargetTracker"}, "target_position"}, false);
        
        // 声明输出
        declareOutput<std::vector<double>>("guidance_command_inertial");
        declareOutput<std::vector<double>>("guidance_command_body");
        declareOutput<double>("range_to_target");
    }

    std::string getComponentType() const override {
        return "SimpleGuidanceWithTransform";
    }

protected:
    void updateImpl() override {
        using namespace gnc::coordination;
        
        // 获取输入状态
        auto pos_inertial = getState<Vector3d>("position_inertial");
        auto vel_inertial = getState<Vector3d>("velocity_inertial");
        
        // ===== 超简化坐标转换示例 =====
        
        // 1. 一行代码转换到载体系
        auto pos_body = TRANSFORM_VEC(pos_inertial, "INERTIAL", "BODY");
        auto vel_body = TRANSFORM_VEC(vel_inertial, "INERTIAL", "BODY");
        
        // 2. 在载体系中进行制导计算
        std::vector<double> cmd_body = computeBodyGuidanceCommand(pos_body, vel_body);
        
        // 3. 一行代码转回惯性系
        auto cmd_inertial = TRANSFORM_VEC(cmd_body, "BODY", "INERTIAL");
        
        // 输出结果
        setState("guidance_command_body", cmd_body);
        setState("guidance_command_inertial", cmd_inertial);
        
        // 计算到目标的距离（如果有目标）
        try {
            auto target_pos = getState<Vector3d>("target_position");
            
            // 计算相对位置并转换到载体系
            Vector3d rel_pos_inertial = {
                target_pos[0] - pos_inertial[0],
                target_pos[1] - pos_inertial[1],
                target_pos[2] - pos_inertial[2]
            };
            
            // 一行代码获取载体系相对位置
            auto rel_pos_body = TRANSFORM_VEC(rel_pos_inertial, "INERTIAL", "BODY");
            
            // 计算距离
            double range = std::sqrt(rel_pos_body[0]*rel_pos_body[0] + 
                                   rel_pos_body[1]*rel_pos_body[1] + 
                                   rel_pos_body[2]*rel_pos_body[2]);
            setState("range_to_target", range);
            
            LOG_COMPONENT_DEBUG("Target in body frame: [{:.1f}, {:.1f}, {:.1f}] m, Range: {:.1f} m",
                               rel_pos_body[0], rel_pos_body[1], rel_pos_body[2], range);
            
        } catch (const std::exception&) {
            setState("range_to_target", -1.0);
        }
    }

private:
    /**
     * @brief 在载体系中计算制导指令
     */
    std::vector<double> computeBodyGuidanceCommand(const Vector3d& pos_body,
                                                   const Vector3d& vel_body) {
        // 简单的比例导引示例
        // 在载体系中，目标通常在前方（正X方向）
        Vector3d desired_vel_body = {100.0, 0.0, 0.0}; // 期望速度
        
        // 速度误差
        Vector3d vel_error = {
            desired_vel_body[0] - vel_body[0],
            desired_vel_body[1] - vel_body[1],
            desired_vel_body[2] - vel_body[2]
        };
        
        // 简单的P控制
        const double kp = 0.5;
        std::vector<double> cmd_body = {
            kp * vel_error[0],
            kp * vel_error[1],
            kp * vel_error[2]
        };
        
        // 限幅
        const double max_cmd = 20.0; // m/s^2
        double cmd_mag = std::sqrt(cmd_body[0]*cmd_body[0] + 
                                  cmd_body[1]*cmd_body[1] + 
                                  cmd_body[2]*cmd_body[2]);
        
        if (cmd_mag > max_cmd) {
            double scale = max_cmd / cmd_mag;
            cmd_body[0] *= scale;
            cmd_body[1] *= scale;
            cmd_body[2] *= scale;
        }
        
        return cmd_body;
    }
};

// 注册组件
static gnc::ComponentRegistrar<SimpleGuidanceWithTransform> simple_guidance_with_transform_registrar("SimpleGuidanceWithTransform");

} // namespace components
} // namespace gnc