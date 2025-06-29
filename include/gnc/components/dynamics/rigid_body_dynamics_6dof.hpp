#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 演示一个核心动力学模型，它消费的是可能被拉偏后的力
class RigidBodyDynamics6DoF : public states::ComponentBase {
public:
    RigidBodyDynamics6DoF(states::VehicleId id) : states::ComponentBase(id, "Dynamics") {
        // 依赖的是经过适配器拉偏后的气动力
        declareInput<std::vector<double>>("aero_force_truth_N", { {id, "Aerodynamics"}, "aero_force_truth_N" });
        
        // 输出真值状态
        declareOutput<std::vector<double>>("position_truth_m", std::vector<double>{0.0, 0.0, 0.0});
        declareOutput<std::vector<double>>("velocity_truth_mps", std::vector<double>{0.0, 0.0, 0.0});
        declareOutput<std::vector<double>>("attitude_truth_quat", std::vector<double>{1.0, 0.0, 0.0, 0.0}); // w,x,y,z
    }

    std::string getComponentType() const override {
        return "RigidBodyDynamics6DoF";
    }
protected:
    void updateImpl() override {
        // 伪实现：简单积分
        position_[0] += velocity_[0] * 0.01;
        position_[1] += velocity_[1] * 0.01;
        position_[2] += velocity_[2] * 0.01;
        
        setState("position_truth_m", position_);
        setState("velocity_truth_mps", velocity_);
        setState("attitude_truth_quat", attitude_);
        LOG_COMPONENT_DEBUG("Updated truth state. Position X: {}", position_[0]);
    }
private:
    std::vector<double> position_{0.0, 0.0, 0.0};
    std::vector<double> velocity_{100.0, 0.0, 0.0};
    std::vector<double> attitude_{1.0, 0.0, 0.0, 0.0}; // 单位四元数
};

static gnc::ComponentRegistrar<RigidBodyDynamics6DoF> rigid_body_dynamics_6dof_registrar("RigidBodyDynamics6DoF");

}