#ifndef GNC_CORE_SIMULATOR_HPP_
#define GNC_CORE_SIMULATOR_HPP_

#include "gnc/core/state_manager.hpp"
#include <memory>

namespace gnc {
namespace core {

/**
 * @brief Manages the overall simulation lifecycle.
 * 
 * This class is responsible for initializing the simulation environment,
 * running the simulation loop, and cleaning up resources. It encapsulates
 * the core logic of setting up components from configuration and executing
 * the time steps.
 */
class Simulator {
public:
    Simulator();
    ~Simulator();

    // Disable copy and move semantics to ensure single ownership
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&) = delete;
    Simulator& operator=(Simulator&&) = delete;

    /**
     * @brief Initializes the simulator.
     * 
     * Reads configuration, creates all vehicles and their components,
     * registers them with the StateManager, and validates dependencies.
     * This must be called before run() or step().
     */
    void initialize();

    /**
     * @brief Runs the entire simulation for a predefined number of steps.
     */
    void run();

    /**
     * @brief Executes a single simulation step.
     * @param dt The time delta for the step (currently unused, for future enhancement).
     */
    void step(double dt = 0.01);

private:
    std::unique_ptr<StateManager> state_manager_;
    bool is_initialized_ = false;
};

} // namespace core
} // namespace gnc

#endif // GNC_CORE_SIMULATOR_HPP_
