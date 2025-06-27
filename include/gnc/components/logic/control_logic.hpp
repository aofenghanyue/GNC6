#pragma once
#include "../../core/component_base.hpp"
#include <vector>

namespace gnc::components {
// 控制逻辑组件
class ControlLogic : public states::ComponentBase {
public:
    ControlLogic(states::VehicleId id) : states::ComponentBase(id, "Control") {
        declareInput<double>("throttle_cmd", {{id, "Guidance"}, "desired_throttle_level"});
        declareOutput<double>("engine_gimbal_angle_rad");
    }
protected:
    void updateImpl() override {
        auto throttle = getState<double>({{getVehicleId(), "Guidance"}, "desired_throttle_level"});
        setState("engine_gimbal_angle_rad", throttle * 0.1); // 伪实现
        std::cout << "    [Control] Output gimbal angle." << std::endl;
    }
};
}