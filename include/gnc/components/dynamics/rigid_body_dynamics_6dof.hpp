#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include "../../coordination/coordination.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"
#include <math/transform/transform.hpp>

using namespace gnc::math::transform;
namespace gnc::components {

// 演示一个核心动力学模型，它消费的是可能被拉偏后的力
class RigidBodyDynamics6DoF : public states::ComponentBase {
public:
    RigidBodyDynamics6DoF(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Dynamics", instanceName) {
        // 全局依赖
        declareInput<double>("timing_current_s", {{globalId, "TimingManager"}, "timing_current_s"});
        declareInput<bool>("coordination_initialized",{{globalId,"SimpleCoordinationInitializer"}, "coordination_initialized"});

        // 依赖的是经过适配器拉偏后的气动力
        declareInput<std::vector<double>>("aero_force_truth_N", { {id, "Aerodynamics"}, "aero_force_truth_N" });
        
        // 输出真值状态
        declareOutput<std::vector<double>>("position_truth_m", std::vector<double>{0.0, 0.0, 0.0});
        declareOutput<std::vector<double>>("velocity_truth_mps", std::vector<double>{0.0, 0.0, 0.0});
        declareOutput<std::vector<double>>("attitude_truth_quat", std::vector<double>{1.0, 0.0, 0.0, 0.0}); // w,x,y,z
        
        // 新增：输出载体系速度
        declareOutput<std::vector<double>>("velocity_body_mps", std::vector<double>{0.0, 0.0, 0.0});
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
        
        double t = getState<double>({{globalId, "TimingManager"}, "timing_current_s"});
        Transform transform_V_B = Transform(10*t*EIGEN_PI/180.0,0,0,EulerSequence::ZYX);
        attitude_[0] = transform_V_B.asQuaternion().coeffs()[0];
        attitude_[1] = transform_V_B.asQuaternion().coeffs()[1];
        attitude_[2] = transform_V_B.asQuaternion().coeffs()[2];
        attitude_[3] = transform_V_B.asQuaternion().coeffs()[3];
        
        setState("position_truth_m", position_);
        setState("velocity_truth_mps", velocity_);
        setState("attitude_truth_quat", attitude_);
        
        // 使用超简化接口计算载体系速度 - 只需一行代码！
        setState("velocity_body_mps", SAFE_TRANSFORM_VEC(velocity_, "INERTIAL", "BODY"));
        
        LOG_COMPONENT_DEBUG("Updated truth state. Position X: {}", position_[0]);
        LOG_COMPONENT_DEBUG("Velocity in body frame: {}, {}, {}", 
            getState<std::vector<double>>("velocity_body_mps")[0],
            getState<std::vector<double>>("velocity_body_mps")[1],
            getState<std::vector<double>>("velocity_body_mps")[2]);
    }
private:
    std::vector<double> position_{0.0, 0.0, 0.0};
    std::vector<double> velocity_{100.0, 0.0, 0.0};
    std::vector<double> attitude_{1.0, 0.0, 0.0, 0.0}; // 单位四元数
};

static gnc::ComponentRegistrar<RigidBodyDynamics6DoF> rigid_body_dynamics_6dof_registrar("RigidBodyDynamics6DoF");

}