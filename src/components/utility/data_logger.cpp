/**
 * @file data_logger.cpp
 * @brief DataLogger component implementation
 */

#include "gnc/components/utility/data_logger.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include <regex>
#include <stdexcept>


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

    try {
        // Get configuration from utility.yaml using ConfigManager
        auto& config_manager = ConfigManager::getInstance();
        auto utility_config = config_manager.getConfig(ConfigFileType::UTILITY);
        
        // Check if DataLogger configuration exists
        if (!utility_config.contains("utility") || !utility_config["utility"].contains("data_logger")) {
            LOG_COMPONENT_WARN("DataLogger configuration not found in utility.yaml, using defaults");
            return;
        }
        
        auto data_logger_config = utility_config["utility"]["data_logger"];
        
        // Load basic configuration parameters
        try {
            output_format_ = data_logger_config.value("format", "hdf5");
            LOG_COMPONENT_DEBUG("Loaded format: {}", output_format_);
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Error loading format: {}", e.what());
            output_format_ = "hdf5";
        }
        
        try {
            file_path_ = data_logger_config.value("file_path", "logs/datalogger_output.h5");
            LOG_COMPONENT_DEBUG("Loaded file_path: {}", file_path_);
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Error loading file_path: {}", e.what());
            file_path_ = "logs/datalogger_output.h5";
        }
        
        try {
            log_frequency_hz_ = data_logger_config.value("log_frequency_hz", 0.0);
            LOG_COMPONENT_DEBUG("Loaded log_frequency_hz: {}", log_frequency_hz_);
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Error loading log_frequency_hz: {}", e.what());
            log_frequency_hz_ = 0.0;
        }
        
        try {
            log_metadata_ = data_logger_config.value("log_metadata", true);
            LOG_COMPONENT_DEBUG("Loaded log_metadata: {}", log_metadata_);
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Error loading log_metadata: {}", e.what());
            log_metadata_ = true;
        }
        
        // Validate output format
        if (output_format_ != "hdf5" && output_format_ != "csv") {
            LOG_COMPONENT_WARN("Invalid output format '{}', defaulting to 'hdf5'", output_format_);
            output_format_ = "hdf5";
        }
        
        // Update file extension if needed
        if (output_format_ == "csv" && file_path_.find(".h5") != std::string::npos) {
            file_path_ = file_path_.substr(0, file_path_.find_last_of('.')) + ".csv";
        } else if (output_format_ == "hdf5" && file_path_.find(".csv") != std::string::npos) {
            file_path_ = file_path_.substr(0, file_path_.find_last_of('.')) + ".h5";
        }
        
        // Load selectors configuration
        selectors_.clear();
        if (data_logger_config.contains("selectors") && data_logger_config["selectors"].is_array()) {
            LOG_COMPONENT_DEBUG("Processing {} selectors", data_logger_config["selectors"].size());
            
            for (size_t i = 0; i < data_logger_config["selectors"].size(); ++i) {
                try {
                    const auto& selector_config = data_logger_config["selectors"][i];
                    LOG_COMPONENT_DEBUG("Processing selector {}: {}", i, selector_config.dump());
                    
                    StateSelector selector;
                    
                    if (selector_config.contains("state")) {
                        // Specific state selector
                        selector.state = selector_config["state"].get<std::string>();
                        selectors_.push_back(selector);
                        LOG_COMPONENT_DEBUG("Added specific state selector: {}", selector.state);
                    } else if (selector_config.contains("component_regex")) {
                    // Regex-based selector
                    selector.component_regex = selector_config["component_regex"].get<std::string>();
                    selector.state_regex = selector_config.value("state_regex", ".*");
                    selector.exclude_state_regex = selector_config.value("exclude_state_regex", "");
                    
                    // Validate regex patterns
                    try {
                        std::regex comp_regex(selector.component_regex);
                        std::regex state_regex(selector.state_regex);
                        if (!selector.exclude_state_regex.empty()) {
                            std::regex exclude_regex(selector.exclude_state_regex);
                        }
                        
                        selectors_.push_back(selector);
                        LOG_COMPONENT_DEBUG("Added regex selector - Component: '{}', State: '{}', Exclude: '{}'", 
                                          selector.component_regex, selector.state_regex, selector.exclude_state_regex);
                    } catch (const std::regex_error& e) {
                        LOG_COMPONENT_WARN("Invalid regex pattern in selector, skipping: {}", e.what());
                    }
                } else {
                    LOG_COMPONENT_WARN("Invalid selector configuration, missing 'state' or 'component_regex'");
                }
                } catch (const std::exception& e) {
                    LOG_COMPONENT_ERROR("Error processing selector {}: {}", i, e.what());
                }
            }
        }
        
        // If no selectors configured, add a default one to log timing
        if (selectors_.empty()) {
            LOG_COMPONENT_WARN("No selectors configured, adding default timing selector");
            selectors_.push_back(StateSelector("TimingManager.timing_current_s"));
        }
        
        LOG_COMPONENT_INFO("Configuration loaded successfully - Format: {}, Path: {}, Frequency: {} Hz, Selectors: {}", 
                          output_format_, file_path_, log_frequency_hz_, selectors_.size());
        
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Failed to load configuration: {}", e.what());
        LOG_COMPONENT_WARN("Using default configuration values");
        
        // Set safe defaults
        output_format_ = "hdf5";
        file_path_ = "logs/datalogger_output.h5";
        log_frequency_hz_ = 0.0;
        log_metadata_ = true;
        selectors_.clear();
        selectors_.push_back(StateSelector("TimingManager.timing_current_s"));
    }
    
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