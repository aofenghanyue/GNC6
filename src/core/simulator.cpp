#include "gnc/core/simulator.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/core/component_factory.hpp"
#include <iostream>

// Include all component headers for self-registration
#include "gnc/components/environment/simple_atmosphere.hpp"
#include "gnc/components/effectors/simple_aerodynamics.hpp"
#include "gnc/components/dynamics/rigid_body_dynamics_6dof.hpp"
#include "gnc/components/sensors/ideal_imu_sensor.hpp"
#include "gnc/components/logic/navigation_logic.hpp"
#include "gnc/components/logic/control_logic.hpp"
#include "gnc/components/logic/phased_guidance_logic.hpp"
#include "gnc/components/logic/guidance_logic.hpp"

// Include the new TimingManager for global component creation
#include "gnc/core/timing_manager.hpp"

namespace gnc {
namespace core {

using namespace gnc::components;
using namespace gnc::states;

Simulator::Simulator()
    : state_manager_(std::make_unique<StateManager>()) {
    LOG_INFO("Simulator created. Call initialize() to set up.");
}

Simulator::~Simulator() {
    LOG_INFO("Simulator shutting down.");
}

void Simulator::initialize() {
    if (is_initialized_) {
        LOG_WARN("Simulator is already initialized.");
        return;
    }

    LOG_INFO("GNC Simulation Framework Initializing...");

    auto& config_manager = utility::ConfigManager::getInstance();

    // Load vehicle-specific components from config
    auto vehicles_config = config_manager.getConfig(utility::ConfigFileType::CORE)["core"]["vehicles"];
    for (const auto& vehicle_config : vehicles_config) {
        VehicleId vehicle_id = vehicle_config["id"].get<VehicleId>();
        LOG_INFO("Loading components for Vehicle ID: {}", vehicle_id);

        for (const auto& comp_config : vehicle_config["components"]) {
            std::string type_str;
            std::string instance_name;

            if (comp_config.is_string()) {
                type_str = comp_config.get<std::string>();
            } else if (comp_config.is_object()) {
                type_str = comp_config["type"].get<std::string>();
                if (comp_config.contains("name")) {
                    instance_name = comp_config["name"].get<std::string>();
                }
            }
            ComponentBase* component = ComponentFactory::getInstance().createComponent(type_str, vehicle_id, instance_name);
            state_manager_->registerComponent(component);
        }
    }

    // Finalize setup
    state_manager_->validateAndSortComponents();
    is_initialized_ = true;
    LOG_INFO("Simulator initialization complete.");
}

void Simulator::step() {
    if (!is_initialized_) {
        LOG_ERROR("Cannot step simulation before it is initialized.");
        return;
    }
    state_manager_->updateAll(); 
}

void Simulator::run() {
    if (!is_initialized_) {
        LOG_ERROR("Cannot run simulation before it is initialized. Call initialize() first.");
        return;
    }

    LOG_INFO("Starting data-driven simulation loop...");

    // The ID for the state that controls the loop
    const StateId should_run_id {
        ComponentId{globalId, "TimingManager"}, // Vehicle 0, default name
        "timing_should_run"
    };

    // Loop continues as long as the TimingManager says it should
    while (state_manager_->getState<bool>(should_run_id)) {
        step();
    }

    LOG_INFO("Simulation loop finished.");
}

} // namespace core
} // namespace gnc