#ifndef GNC_CORE_TIMING_MANAGER_HPP_
#define GNC_CORE_TIMING_MANAGER_HPP_
#include "gnc/core/component_registrar.hpp"
#include "gnc/core/component_base.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"

using namespace gnc::components::utility;

namespace gnc {
namespace core {

/**
 * @brief Manages the simulation's lifecycle, time, and step control.
 * 
 * This component acts as the central clock and controller for the simulation.
 * It reads its configuration (duration, time step) and drives the simulation
 * by providing key state outputs that the Simulator loop consumes.
 * 
 * It is intended to be a singleton component, typically registered under a
 * special vehicle ID (e.g., 0) representing the global simulation context.
 * 
 * Declared Output States:
 * - "simulation.time.current_s" (double): Total elapsed simulation time in seconds.
 * - "simulation.time.delta_s" (double): The time step for the current frame in seconds.
 * - "simulation.time.frame_count" (uint64_t): Total number of frames executed.
 * - "simulation.control.should_run" (bool): Flag to signal if the simulation should continue.
 */
class TimingManagerComponent : public states::ComponentBase {
public:
    TimingManagerComponent(states::VehicleId vehicleId, const std::string& instanceName = "");

    std::string getComponentType() const override {
        return "TimingManager";
    }

    /**
     * @brief Reads simulation parameters from config and sets initial state.
     */
    void initialize() override;

protected:
    /**
     * @brief Updates the simulation time and checks for termination conditions.
     */
    void updateImpl() override;

private:
    // Configuration parameters
    double duration_s_ = 10.0; // Default duration
    double time_step_s_ = 1; // Default time step

    // Internal state
    double current_time_s_ = 0.0;
    uint64_t frame_count_ = 0;
    bool should_run_ = true;
};

TimingManagerComponent::TimingManagerComponent(VehicleId vehicleId, const std::string& instanceName)
    : ComponentBase(vehicleId, "TimingManager", instanceName) {
    
    // Declare the states this component will provide to the system
    declareOutput<double>("timing_current_s");
    declareOutput<double>("timing_delta_s");
    declareOutput<uint64_t>("timing_frame_count");
    declareOutput<bool>("timing_should_run");
}

void TimingManagerComponent::initialize() {
    LOG_INFO("Initializing TimingManager...");

    try {
        auto& config_manager = ConfigManager::getInstance();
        // 这里直接使用auto core_config = config_manager.getConfig(ConfigFileType::CORE)["core"]["timing"];会读取不到参数，编译的问题...
        // 需要将链式读取分为多步
        const auto& core_config = config_manager.getConfig(ConfigFileType::CORE);

        if (core_config.contains("core") && core_config["core"].contains("timing")) {
            const auto& sim_config = core_config["core"]["timing"];
            duration_s_ = sim_config.value("duration_s", 10.0);
            time_step_s_ = sim_config.value("time_step_s", 1.0);
            LOG_INFO("Simulation configured for a duration of {}s with a {}s time step.", duration_s_, time_step_s_);
        } else {
            LOG_WARN("Config key 'core.timing' not found. Using default values.");
        }
    } catch (const std::exception& e) {
        LOG_WARN("Could not find simulation settings in core.yaml. Using default values. Details: {}", e.what());
    }

    // Set the initial state values
    setState("timing_current_s", current_time_s_);
    setState("timing_delta_s", time_step_s_);
    setState("timing_frame_count", frame_count_);
    setState("timing_should_run", should_run_);
}

void TimingManagerComponent::updateImpl() {
    // Increment frame count and time
    frame_count_++;
    current_time_s_ += time_step_s_;

    // Check if the simulation duration has been reached
    if (current_time_s_ >= duration_s_) {
        should_run_ = false;
        LOG_INFO("Simulation duration of {}s reached. Halting simulation.", duration_s_);
    }

    // Update the states for other components to use in the next frame
    setState("timing_current_s", current_time_s_);
    setState("timing_frame_count", frame_count_);
    setState("timing_should_run", should_run_);
    // a simple impliementation just assume delta_s is constant, so it doesn't need to be updated every frame
}

static gnc::ComponentRegistrar<TimingManagerComponent> timing_manager_registrar("TimingManager");
} // namespace core
} // namespace gnc

#endif // GNC_CORE_TIMING_MANAGER_HPP_
