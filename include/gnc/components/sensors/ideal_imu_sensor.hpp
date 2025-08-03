#pragma once
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include <vector>
#include "../utility/simple_logger.hpp"

namespace gnc::components {

// 传感器组件，消费真值，输出带偏置的测量值
class IdealIMUSensor : public states::ComponentBase {
public:
    IdealIMUSensor(states::VehicleId id, const std::string& instanceName = "") 
        : states::ComponentBase(id, "IMU_Sensor", instanceName) {
        declareOutput<Vector3d>("measured_acceleration");
    }

    std::string getComponentType() const override {
        return "IdealIMUSensor";
    }
protected:
    void updateImpl() override {
        auto vel_truth = getState<Vector3d>({{getVehicleId(), "Dynamics"}, "velocity_truth_mps"});
        // 伪实现：理想传感器，直接输出一个常量
        Vector3d accel_measured = {0.1, 0.0, -9.8};
        setState("measured_acceleration", accel_measured);
        LOG_COMPONENT_DEBUG("Output measured acceleration: [{}, {}, {}]", 
                  accel_measured[0], accel_measured[1], accel_measured[2]);
    }
};

static gnc::ComponentRegistrar<IdealIMUSensor> ideal_imu_sensor_registrar("IdealIMUSensor");

}