#pragma once
#include "../../core/component_base.hpp"
#include <vector> // 仅作类型示例

namespace gnc::components {

// 演示一个有输入的效应器组件
class SimpleAerodynamics : public states::ComponentBase {
public:
    SimpleAerodynamics(states::VehicleId id) : states::ComponentBase(id, "Aerodynamics") {
        // 依赖大气密度和飞行器速度（来自理想传感器）
        declareInput<double>("air_density_kg_m3", { {id, "Atmosphere"}, "air_density_kg_m3" });
        // declareInput<std::vector<double>>("velocity_truth_mps", { {id, "Dynamics"}, "velocity_truth_mps" }, false);
        
        declareOutput<std::vector<double>>("aero_force_truth_N");
    }
protected:
    void updateImpl() override {
        auto density = getState<double>({ {getVehicleId(), "Atmosphere"}, "air_density_kg_m3" });
        auto velocity = getState<std::vector<double>>({ {getVehicleId(), "Dynamics"}, "velocity_truth_mps" });
        
        // 伪实现：简单阻力模型
        double speed_sq = velocity[0]*velocity[0] + velocity[1]*velocity[1] + velocity[2]*velocity[2];
        double drag = -0.5 * density * speed_sq * 0.1 /*CdA*/;
        std::vector<double> force = {drag, 0.0, 0.0}; // 假设沿X轴负方向
        
        setState("aero_force_truth_N", force);
        std::cout << "    [Aerodynamics] Calculated aero force (truth): " << force[0] << std::endl;
    }
};

}