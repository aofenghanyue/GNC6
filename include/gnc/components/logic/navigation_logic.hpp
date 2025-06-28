#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 导航组件（此处简化为直通）
class PerfectNavigation : public states::ComponentBase {
public:
    PerfectNavigation(states::VehicleId id) : states::ComponentBase(id, "Navigation") {
        // declareInput<std::vector<double>>("pos_truth", {{id, "Dynamics"}, "position_truth_m"}, false);
        declareInput<std::vector<double>>("pos_measured", {{id, "IMU_Sensor"}, "measured_acceleration"});
        declareOutput<std::vector<double>>("pva_estimate");
    }

    std::string getComponentType() const override {
        return "PerfectNavigation";
    }
protected:
    void updateImpl() override {
        auto pos_truth = getState<std::vector<double>>({{getVehicleId(), "Dynamics"}, "position_truth_m"});
        setState("pva_estimate", pos_truth); // 伪实现
        LOG_COMPONENT_DEBUG("Generated perfect PVA estimate");
    }
};

static gnc::ComponentRegistrar<PerfectNavigation> perfect_navigation_registrar("PerfectNavigation");

}