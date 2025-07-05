#pragma once
#include "gnc/core/component_base.hpp"
#include "gnc/core/component_registrar.hpp"
#include "gnc/coordination/coordination.hpp"
#include "../custom/coordination_initializer.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"
#include "math/math.hpp"

namespace gnc::components {

// 演示一个核心动力学模型，它消费的是可能被拉偏后的力
class RigidBodyDynamics6DoF : public states::ComponentBase {
public:
    RigidBodyDynamics6DoF(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Dynamics", instanceName) {
        // 全局依赖
        declareInput<double>("timing_current_s", {{globalId, "TimingManager"}, "timing_current_s"});
        declareInput<bool>("coordination_initialized",{{globalId,"CoordinationInitializer"}, "coordination_initialized"});

        // 依赖的本来是经过适配器拉偏后的气动力，但是这里先不考虑disturb模块
        declareInput<Vector3d>("aero_force_truth_N", { {id, "Aerodynamics"}, "aero_force_truth_N" });
        
        // 输出真值状态
        declareOutput<Vector3d>("position_truth_m", Vector3d(0.0, 0.0, 0.0));
        declareOutput<Vector3d>("velocity_truth_mps", Vector3d(0.0, 0.0, 0.0));
        declareOutput<Quaterniond>("attitude_truth_quat", Quaterniond(1.0, 0.0, 0.0, 0.0)); // w,x,y,z
        
        // 新增：输出载体系速度
        declareOutput<Vector3d>("velocity_body_mps", Vector3d(0.0, 0.0, 0.0));
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
        attitude_ = transform_V_B.asQuaternion();
        
        setState("position_truth_m", position_);
        setState("velocity_truth_mps", velocity_);
        setState("attitude_truth_quat", attitude_);
        
        // 使用超简化接口计算载体系速度 - 只需一行代码！
        Vector3d velocity_body_mps = SAFE_TRANSFORM_VEC(velocity_, "INERTIAL", "BODY");
        setState("velocity_body_mps", velocity_body_mps);
        
        LOG_COMPONENT_DEBUG("Updated truth state. Position X: {}", position_[0]);
        LOG_COMPONENT_DEBUG("Velocity in body frame: {}, {}, {}", 
            getState<Vector3d>("velocity_body_mps")[0],
            getState<Vector3d>("velocity_body_mps")[1],
            getState<Vector3d>("velocity_body_mps")[2]);
        LOG_COMPONENT_DEBUG("Attitude in body frame: {}, {}, {}, {}", 
            getState<Quaterniond>("attitude_truth_quat").w(),
            getState<Quaterniond>("attitude_truth_quat").x(),
            getState<Quaterniond>("attitude_truth_quat").y(),
            getState<Quaterniond>("attitude_truth_quat").z());
    }
private:
    Vector3d position_{0.0, 0.0, 0.0};
    Vector3d velocity_{100.0, 0.0, 0.0};
    Quaterniond attitude_{1.0, 0.0, 0.0, 0.0}; // 单位四元数
};

static gnc::ComponentRegistrar<RigidBodyDynamics6DoF> rigid_body_dynamics_6dof_registrar("RigidBodyDynamics6DoF");

}