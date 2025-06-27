#pragma once

#include "../../core/component_base.hpp"
#include <vector>

namespace gnc::components {

// 制导逻辑组件
class GuidanceLogic : public states::ComponentBase {
public:
    GuidanceLogic(states::VehicleId id) : states::ComponentBase(id, "Guidance") {
        declareInput<std::vector<double>>("nav_pva", {{id, "Navigation"}, "pva_estimate"});
        declareOutput<double>("desired_throttle_level"); // 输出一个油门指令
    }
protected:
    void updateImpl() override {
        setState("desired_throttle_level", 0.75); // 伪实现
        std::cout << "    [Guidance] Output desired throttle: 0.75" << std::endl;
    }
};
}