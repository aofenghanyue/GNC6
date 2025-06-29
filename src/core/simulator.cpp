#include "gnc/core/simulator.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/core/component_factory.hpp"
#include <iostream>

// Include all component headers to ensure self-registration
#include "gnc/components/environment/simple_atmosphere.hpp"
#include "gnc/components/effectors/simple_aerodynamics.hpp"
#include "gnc/components/dynamics/rigid_body_dynamics_6dof.hpp"
#include "gnc/components/sensors/ideal_imu_sensor.hpp"
#include "gnc/components/logic/navigation_logic.hpp"
#include "gnc/components/logic/control_logic.hpp"
#include "gnc/components/logic/phased_guidance_logic.hpp"
#include "gnc/components/logic/guidance_logic.hpp"

namespace gnc {
namespace core {

using namespace gnc::components;

Simulator::Simulator()
    : state_manager_(std::make_unique<StateManager>()) {
    LOG_INFO("Simulator created. Call initialize() to set up the simulation.");
}

Simulator::~Simulator() {
    LOG_INFO("Simulator shutting down.");
    // The unique_ptr for state_manager_ will handle cleanup of components.
}

void Simulator::initialize() {
    if (is_initialized_) {
        LOG_WARN("Simulator is already initialized.");
        return;
    }

    LOG_INFO("GNC Simulation Framework Initializing...");

    // ConfigManager is a singleton, getInstance will load it if needed.
    auto& config_manager = utility::ConfigManager::getInstance();

    auto vehicles_config = config_manager.getConfig(utility::ConfigFileType::CORE)["core"]["vehicles"];

    for (const auto& vehicle_config : vehicles_config) {
        states::VehicleId vehicle_id = vehicle_config["id"].get<states::VehicleId>();
        LOG_INFO("Loading components for Vehicle ID: {}", vehicle_id);

        for (const auto& comp_config : vehicle_config["components"]) {
            std::string type_str;
            std::string instance_name;

            if (comp_config.is_string()) {
                type_str = comp_config.get<std::string>();
                LOG_DEBUG("Creating component of type: {}", type_str.c_str());
            } else if (comp_config.is_object()) {
                type_str = comp_config["type"].get<std::string>();
                if (comp_config.contains("name")) {
                    instance_name = comp_config["name"].get<std::string>();
                    LOG_DEBUG("Creating component of type: {} with name: {}", type_str.c_str(), instance_name.c_str());
                } else {
                    LOG_DEBUG("Creating component of type: {}", type_str.c_str());
                }
            }

            ComponentBase* component = ComponentFactory::getInstance().createComponent(type_str, vehicle_id, instance_name);
            state_manager_->registerComponent(component);
        }
    }

    state_manager_->validateAndSortComponents();
    is_initialized_ = true;
    LOG_INFO("Simulator initialization complete.");
}

void Simulator::step(double dt) {
    if (!is_initialized_) {
        LOG_ERROR("Cannot step simulation before it is initialized.");
        return;
    }
    // In the future, dt will be passed to the updateAll method.
    state_manager_->updateAll(); 
}

void Simulator::run() {
    if (!is_initialized_) {
        LOG_ERROR("Cannot run simulation before it is initialized. Call initialize() first.");
        return;
    }

    LOG_INFO("Starting simulation loop...");
    // Example loop, this will be driven by the new TimeManagerComponent later.
    for (int i = 0; i < 10; ++i) { 
        step();
    }
    LOG_INFO("Simulation loop finished.");
}

} // namespace core
} // namespace gnc
