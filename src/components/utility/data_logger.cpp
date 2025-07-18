/**
 * @file data_logger.cpp
 * @brief DataLogger component implementation
 */

#include "gnc/components/utility/data_logger.hpp"
#include "gnc/components/utility/simple_logger.hpp"


namespace gnc {
namespace components {
namespace utility {



// ============================================================================
// DataLogger Implementation
// ============================================================================

std::string DataLogger::getComponentType() const {
    return "DataLogger";
}

void DataLogger::initialize() {
    if (initialized_) {
        LOG_COMPONENT_WARN("DataLogger already initialized, skipping");
        return;
    }

    try {
        LOG_COMPONENT_INFO("Initializing DataLogger component");

        // Load configuration from utility.yaml
        loadConfiguration();

        // Discover and select states based on configuration
        discoverAndSelectStates();

        // TODO: Create and initialize file writer (Task 4-6)
        // TODO: Set up metadata collection (Task 7)

        initialized_ = true;
        LOG_COMPONENT_INFO("DataLogger initialization completed successfully");
        LOG_COMPONENT_INFO("Output format: {}, File path: {}", output_format_.c_str(), file_path_.c_str());
        LOG_COMPONENT_INFO("Log frequency: {} Hz, Metadata: {}", 
                           log_frequency_hz_, log_metadata_ ? "enabled" : "disabled");
        LOG_COMPONENT_INFO("Selected {} states for logging", states_to_log_.size());

    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Failed to initialize DataLogger: {}", e.what());
        throw;
    }
}

void DataLogger::finalize() {
    if (!initialized_) {
        return;
    }

    try {
        LOG_COMPONENT_INFO("Finalizing DataLogger component");

        // TODO: Flush data buffers and close file handles (Task 10)

        initialized_ = false;
        LOG_COMPONENT_INFO("DataLogger finalization completed");

    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error during DataLogger finalization: {}", e.what());
    }
}

void DataLogger::updateImpl() {
    if (!initialized_) {
        return;
    }

    try {
        // TODO: Get current time from TimingManager (Task 9)
        double current_time = 0.0; // Placeholder

        // Check if it's time to log
        if (!shouldLog(current_time)) {
            return;
        }

        // TODO: Collect state values and write data (Task 9)
        
        last_log_time_ = current_time;

    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error during DataLogger update: {}", e.what());
    }
}

void DataLogger::loadConfiguration() {
    LOG_COMPONENT_DEBUG("Loading DataLogger configuration");

    // TODO: Load configuration from utility.yaml using ConfigManager (Task 2)
    // For now, use default values
    
    LOG_COMPONENT_DEBUG("Configuration loaded - Format: {}, Path: {}, Frequency: {} Hz", 
                        output_format_.c_str(), file_path_.c_str(), log_frequency_hz_);
}

void DataLogger::discoverAndSelectStates() {
    LOG_COMPONENT_DEBUG("Discovering and selecting states for logging");

    // TODO: Query StateManager and apply regex matching (Task 3)
    // For now, create empty list
    states_to_log_.clear();
    
    LOG_COMPONENT_DEBUG("State discovery completed, selected {} states", states_to_log_.size());
}

bool DataLogger::shouldLog(double current_time) const {
    // If frequency is 0, log every step
    if (log_frequency_hz_ <= 0.0) {
        return true;
    }

    // Check if enough time has passed based on frequency
    double time_interval = 1.0 / log_frequency_hz_;
    return (current_time - last_log_time_) >= time_interval;
}

} // namespace utility
} // namespace components
} // namespace gnc