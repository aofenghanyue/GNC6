#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {
// 控制逻辑组件
class ControlLogic : public states::ComponentBase {
public:
    ControlLogic(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Control", instanceName) {
        // 简化的组件级依赖声明
        declareInput<void>(ComponentId{id, "GuidanceWithPhase"});
        declareOutput<double>("engine_gimbal_angle_rad");
    }

    std::string getComponentType() const override {
        return "ControlLogic";
    }
protected:
    void updateImpl() override {
        auto throttle = getState<double>({{getVehicleId(), "GuidanceWithPhase"}, "desired_throttle_level"});
        auto throttle2 = get<double>("GuidanceWithPhase.desired_throttle_level");
        setState("engine_gimbal_angle_rad", throttle * 0.1); // 伪实现
        LOG_COMPONENT_DEBUG("Output gimbal angle: {}", throttle * 0.1);
    }
};

static gnc::ComponentRegistrar<ControlLogic> control_logic_registrar("ControlLogic");

}