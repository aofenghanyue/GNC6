#ifndef GNC_CORE_SIMULATOR_HPP_
#define GNC_CORE_SIMULATOR_HPP_

#include "gnc/core/state_manager.hpp"
#include <memory>

namespace gnc {
namespace core {

class Simulator {
public:
    Simulator();
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&) = delete;
    Simulator& operator=(Simulator&&) = delete;

    void initialize();
    void run();
    void step(); // step不再需要dt参数

private:
    std::unique_ptr<StateManager> state_manager_;
    bool is_initialized_ = false;
};

} // namespace core
} // namespace gnc

#endif // GNC_CORE_SIMULATOR_HPP_