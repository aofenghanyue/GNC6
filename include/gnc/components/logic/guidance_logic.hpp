#pragma once

#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 制导逻辑组件
class GuidanceLogic : public states::ComponentBase {
public:
    GuidanceLogic(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "Guidance", instanceName) {
        // 简化的组件级依赖声明
        declareInput<void>(ComponentId{id, "Navigation"});
        declareOutput<double>("desired_throttle_level"); // 输出一个油门指令
    }

    std::string getComponentType() const override {
        return "GuidanceLogic";
    }
protected:
    void updateImpl() override {
        setState("desired_throttle_level", 0.75); // 伪实现
        LOG_COMPONENT_DEBUG("Output desired throttle: 0.75");
    }
};

static gnc::ComponentRegistrar<GuidanceLogic> guidance_logic_registrar("GuidanceLogic");

}