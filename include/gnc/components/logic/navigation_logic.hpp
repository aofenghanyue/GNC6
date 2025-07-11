#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 导航组件（此处简化为直通）
class PerfectNavigation : public states::ComponentBase {
public:
    PerfectNavigation(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Navigation", instanceName) {
        // declareInput<Vector3d>("pos_truth", {{id, "Dynamics"}, "position_truth_m"}, false);
        declareInput<Vector3d>("pos_measured", {{id, "IMU_Sensor"}, "measured_acceleration"});
        declareOutput<Vector3d>("pva_estimate");
    }

    std::string getComponentType() const override {
        return "PerfectNavigation";
    }
protected:
    void updateImpl() override {
        auto pos_measured = getState<Vector3d>({{getVehicleId(), "IMU_Sensor"}, "measured_acceleration"});
        setState("pva_estimate", pos_measured); // 伪实现
        LOG_COMPONENT_DEBUG("Generated perfect PVA estimate");
    }
};

static gnc::ComponentRegistrar<PerfectNavigation> perfect_navigation_registrar("PerfectNavigation");

}