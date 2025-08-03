#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector> // 仅作类型示例
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 演示一个有输入的效应器组件
class SimpleAerodynamics : public states::ComponentBase {
public:
    SimpleAerodynamics(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Aerodynamics", instanceName) {
        // 简化的组件级依赖声明
        declareInput<void>(ComponentId{id, "Atmosphere"});
        declareInput<void>(ComponentId{id, "Disturbance"});
        
        declareOutput<Vector3d>("aero_force_truth_N");
    }

    std::string getComponentType() const override {
        return "SimpleAerodynamics";
    }
protected:
    void updateImpl() override {
        auto density = getState<double>({ {getVehicleId(), "Atmosphere"}, "air_density_kg_m3" });
        auto velocity = getState<Vector3d>({ {getVehicleId(), "Dynamics"}, "velocity_truth_mps" });
        
        // 伪实现：简单阻力模型
        double speed_sq = velocity[0]*velocity[0] + velocity[1]*velocity[1] + velocity[2]*velocity[2];
        double drag = -0.5 * density * speed_sq * 0.1 /*CdA*/;
        LOG_COMPONENT_TRACE("Drag: {}, Drag factor: {}", drag, get<double>("Disturbance.drag_factor"));
        drag *= get<double>("Disturbance.drag_factor");
        LOG_COMPONENT_TRACE("Drag after factor: {}", drag);
        std::vector<double> force = {drag, 0.0, 0.0}; // 假设沿X轴负方向
        
        setState("aero_force_truth_N", force);
        LOG_COMPONENT_DEBUG("Calculated aero force (truth): {}", force[0]);
    }
};

static gnc::ComponentRegistrar<SimpleAerodynamics> simple_aerodynamics_registrar("SimpleAerodynamics");

}