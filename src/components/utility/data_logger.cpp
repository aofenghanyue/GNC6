/**
 * @file data_logger.cpp
 * @brief DataLogger component implementation
 */

#include "gnc/components/utility/data_logger.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/core/state_manager.hpp"
#include <regex>
#include <stdexcept>
#include <any>

using namespace gnc::states;


namespace gnc {
namespace components {
namespace utility {

// ============================================================================
// FileWriter Factory Implementation
// ============================================================================

std::unique_ptr<FileWriter> createFileWriter(const std::string& format) {
    if (format == "csv") {
        // TODO: Implement CSVWriter in task 5
        throw std::runtime_error("CSV writer not yet implemented");
    } else if (format == "hdf5") {
        // TODO: Implement HDF5Writer in task 6
        throw std::runtime_error("HDF5 writer not yet implemented");
    } else {
        throw std::invalid_argument("Unsupported file format: " + format + ". Supported formats: csv, hdf5");
    }
}

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

        // Create file writer using factory (Task 4)
        try {
            file_writer_ = createFileWriter(output_format_);
            LOG_COMPONENT_DEBUG("Created {} file writer", output_format_);
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Failed to create file writer for format '{}': {}", output_format_, e.what());
            throw;
        }

        // TODO: Initialize file writer (Task 5-6)
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

        // Finalize file writer if it exists
        if (file_writer_) {
            try {
                file_writer_->finalize();
                LOG_COMPONENT_DEBUG("File writer finalized successfully");
            } catch (const std::exception& e) {
                LOG_COMPONENT_ERROR("Error finalizing file writer: {}", e.what());
            }
            file_writer_.reset();
        }

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

    states_to_log_.clear();
    
    // Get access to the StateManager through the state access interface
    IStateAccess* state_access = getStateAccess();
    if (!state_access) {
        LOG_COMPONENT_ERROR("StateManager not available for state discovery");
        return;
    }
    
    // Cast to StateManager to access the new methods
    gnc::StateManager* state_manager = dynamic_cast<gnc::StateManager*>(state_access);
    if (!state_manager) {
        LOG_COMPONENT_ERROR("Failed to access StateManager for component discovery");
        return;
    }
    
    // Get all available output states from StateManager
    std::vector<StateId> all_available_states = state_manager->getAllOutputStates();
    LOG_COMPONENT_DEBUG("Found {} total available output states", all_available_states.size());
    
    // Process each selector
    LOG_COMPONENT_DEBUG("Processing {} selectors", selectors_.size());
    
    for (const auto& selector : selectors_) {
        if (!selector.state.empty()) {
            // Handle specific state selector
            processSpecificStateSelector(selector, all_available_states);
        } else if (!selector.component_regex.empty()) {
            // Handle regex-based selector
            processRegexSelector(selector, all_available_states);
        }
    }
    
    LOG_COMPONENT_INFO("State discovery completed, selected {} states for logging", states_to_log_.size());
    
    // Log selected states for debugging
    for (const auto& state_id : states_to_log_) {
        LOG_COMPONENT_DEBUG("Selected state: {}.{}.{}", 
                           state_id.component.vehicleId, 
                           state_id.component.name, 
                           state_id.name);
    }
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

void DataLogger::processSpecificStateSelector(const StateSelector& selector, const std::vector<StateId>& all_available_states) {
    LOG_COMPONENT_DEBUG("Processing specific state selector: {}", selector.state);
    
    try {
        // Parse the state string to extract component and state names
        // Format can be: "state", "Component.state", or "VehicleId.Component.state"
        StateId target_state_id;
        
        size_t first_dot = selector.state.find('.');
        if (first_dot == std::string::npos) {
            // No dot, assume it's a state from self on this vehicle
            target_state_id = {{getVehicleId(), getName()}, selector.state};
        } else {
            size_t second_dot = selector.state.find('.', first_dot + 1);
            if (second_dot == std::string::npos) {
                // One dot: "Component.state" format
                std::string component_name = selector.state.substr(0, first_dot);
                std::string state_name = selector.state.substr(first_dot + 1);
                
                // Use the same vehicle ID as this DataLogger component
                target_state_id = {{getVehicleId(), component_name}, state_name};
            } else {
                // Two dots: "VehicleId.Component.state" format
                std::string vehicle_id_str = selector.state.substr(0, first_dot);
                std::string component_name = selector.state.substr(first_dot + 1, second_dot - first_dot - 1);
                std::string state_name = selector.state.substr(second_dot + 1);
                
                // Parse vehicle ID
                VehicleId vehicle_id;
                try {
                    if (vehicle_id_str.substr(0, 7) == "vehicle") {
                        vehicle_id = static_cast<VehicleId>(std::stoi(vehicle_id_str.substr(7)));
                    } else {
                        vehicle_id = static_cast<VehicleId>(std::stoi(vehicle_id_str));
                    }
                } catch (const std::exception& e) {
                    LOG_COMPONENT_WARN("Invalid vehicle ID in state selector '{}', using current vehicle", selector.state);
                    vehicle_id = getVehicleId();
                }
                
                target_state_id = {{vehicle_id, component_name}, state_name};
            }
        }
        
        // Check if the target state exists in the available states
        bool found = false;
        for (const auto& available_state : all_available_states) {
            if (available_state == target_state_id) {
                states_to_log_.push_back(target_state_id);
                found = true;
                LOG_COMPONENT_DEBUG("Added specific state: {}.{}.{}", 
                                   target_state_id.component.vehicleId, 
                                   target_state_id.component.name, 
                                   target_state_id.name);
                break;
            }
        }
        
        if (!found) {
            LOG_COMPONENT_WARN("Specific state '{}' not found in available states", selector.state);
        }
        
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error processing specific state selector '{}': {}", selector.state, e.what());
    }
}

void DataLogger::processRegexSelector(const StateSelector& selector, const std::vector<StateId>& all_available_states) {
    LOG_COMPONENT_DEBUG("Processing regex selector - Component: '{}', State: '{}', Exclude: '{}'", 
                        selector.component_regex, selector.state_regex, selector.exclude_state_regex);
    
    try {
        // Compile regex patterns
        std::regex component_pattern(selector.component_regex);
        std::regex state_pattern(selector.state_regex);
        std::regex exclude_pattern;
        bool has_exclude = !selector.exclude_state_regex.empty();
        if (has_exclude) {
            exclude_pattern = std::regex(selector.exclude_state_regex);
        }
        
        int matched_count = 0;
        for (const auto& state_id : all_available_states) {
            // Check if component name matches
            if (!std::regex_match(state_id.component.name, component_pattern)) {
                continue;
            }
            
            // Check if state name matches
            if (!std::regex_match(state_id.name, state_pattern)) {
                continue;
            }
            
            // Check exclusion pattern
            if (has_exclude && std::regex_match(state_id.name, exclude_pattern)) {
                LOG_COMPONENT_DEBUG("Excluded state by exclude pattern: {}.{}", 
                                   state_id.component.name, state_id.name);
                continue;
            }
            
            // Add to selected states
            states_to_log_.push_back(state_id);
            matched_count++;
            LOG_COMPONENT_DEBUG("Matched state: {}.{}.{}", 
                               state_id.component.vehicleId, 
                               state_id.component.name, 
                               state_id.name);
        }
        
        LOG_COMPONENT_DEBUG("Regex selector matched {} states", matched_count);
        
    } catch (const std::regex_error& e) {
        LOG_COMPONENT_ERROR("Regex error in selector: {}", e.what());
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error processing regex selector: {}", e.what());
    }
}



} // namespace utility
} // namespace components
} // namespace gnc