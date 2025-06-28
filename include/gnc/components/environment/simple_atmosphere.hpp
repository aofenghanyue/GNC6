#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 演示一个没有输入的纯数据源组件
class SimpleAtmosphere : public states::ComponentBase {
public:
    SimpleAtmosphere(states::VehicleId id) : states::ComponentBase(id, "Atmosphere") {
        declareOutput<double>("air_density_kg_m3");
    }

    std::string getComponentType() const override {
        return "SimpleAtmosphere";
    }
protected:
    void updateImpl() override {
        double density = 1.225; // 海平面标准大气密度
        setState("air_density_kg_m3", density);
        LOG_COMPONENT_DEBUG("Output air_density: {}", density);
    }
};

static gnc::ComponentRegistrar<SimpleAtmosphere> simple_atmosphere_registrar("SimpleAtmosphere");

}