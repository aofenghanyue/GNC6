/**
 * @file data_logger.cpp
 * @brief DataLogger component implementation
 */

#include "gnc/components/utility/data_logger.hpp"
#include "gnc/components/utility/csv_writer.hpp"
#include "gnc/components/utility/hdf5_writer.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/core/state_manager.hpp"
#include "math/math.hpp"
#include <regex>
#include <stdexcept>
#include <any>
#include <unordered_set>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <limits>

// Platform-specific includes for popen/pclose
#ifdef _WIN32
    #include <io.h>
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
#endif

using namespace gnc::states;


namespace gnc {
namespace components {
namespace utility {

// ============================================================================
// FileWriter Factory Implementation
// ============================================================================

std::unique_ptr<FileWriter> createFileWriter(const std::string& format) {
    if (format == "csv") {
        return std::make_unique<CSVWriter>();
    } else if (format == "hdf5") {
        if (!HDF5Writer::isHDF5Available()) {
            LOG_WARN("HDF5 library not available, falling back to CSV format");
            return std::make_unique<CSVWriter>();
        }
        return std::make_unique<HDF5Writer>();
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

        // Collect metadata if enabled (Task 7)
        nlohmann::json metadata_json;
        if (log_metadata_) {
            metadata_json = collectMetadata();
        }

        // Create flattened state IDs for file writer initialization
        std::vector<gnc::states::StateId> flattened_state_ids;
        for (const auto& flattened_state : flattened_states_) {
            // Create a pseudo StateId for the flattened state
            gnc::states::StateId flattened_id = {
                flattened_state.original_state_id.component,
                flattened_state.flattened_name
            };
            flattened_state_ids.push_back(flattened_id);
        }

        // Initialize file writer
        try {
            file_writer_->initialize(file_path_, flattened_state_ids, log_metadata_, metadata_json);
            LOG_COMPONENT_DEBUG("File writer initialized successfully");
        } catch (const std::exception& e) {
            LOG_COMPONENT_ERROR("Failed to initialize file writer: {}", e.what());
            throw;
        }

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
        LOG_COMPONENT_DEBUG("DataLogger already finalized or not initialized");
        return;
    }

    try {
        LOG_COMPONENT_INFO("Finalizing DataLogger component");

        // Requirement 8.1 & 8.2: Flush all data buffers to disk and properly close file handles
        if (file_writer_) {
            try {
                LOG_COMPONENT_DEBUG("Flushing data buffers and closing file handles");
                file_writer_->finalize();
                LOG_COMPONENT_DEBUG("File writer finalized successfully");
            } catch (const std::exception& e) {
                // Requirement 8.3: Ensure proper cleanup of file resources even on errors
                LOG_COMPONENT_ERROR("Error finalizing file writer: {}", e.what());
                // Continue with cleanup even if file writer finalization fails
            }
            
            // Clean up file writer resource
            file_writer_.reset();
            LOG_COMPONENT_DEBUG("File writer resource cleaned up");
        }

        // Clear cached state data to free memory
        states_to_log_.clear();
        flattened_states_.clear();
        selectors_.clear();
        
        // Reset timing state
        last_log_time_ = 0.0;
        
        // Mark as not initialized
        initialized_ = false;
        
        LOG_COMPONENT_INFO("DataLogger finalization completed successfully");

    } catch (const std::exception& e) {
        // Requirement 8.3: Ensure proper cleanup of file resources even on errors
        LOG_COMPONENT_ERROR("Error during DataLogger finalization: {}", e.what());
        
        // Force cleanup even if errors occurred
        try {
            if (file_writer_) {
                file_writer_.reset();
            }
            states_to_log_.clear();
            flattened_states_.clear();
            selectors_.clear();
            initialized_ = false;
            LOG_COMPONENT_WARN("Forced cleanup completed after finalization error");
        } catch (const std::exception& cleanup_error) {
            LOG_COMPONENT_ERROR("Critical error during forced cleanup: {}", cleanup_error.what());
        }
    }
}

void DataLogger::updateImpl() {
    if (!initialized_) {
        return;
    }

    try {
        // Get current time from TimingManager
        double current_time = 0.0;
        try {
            // Try to get timing from the global TimingManager
            StateId timing_state_id = {{globalId, "TimingManager"}, "timing_current_s"};
            current_time = getState<double>(timing_state_id);
        } catch (const std::exception& e) {
            LOG_COMPONENT_DEBUG("Could not get timing from TimingManager: {}", e.what());
            // Use a simple counter as fallback
            static double fallback_time = 0.0;
            fallback_time += 0.01; // Assume 10ms steps
            current_time = fallback_time;
        }

        // Check if it's time to log
        if (!shouldLog(current_time)) {
            return;
        }

        // Collect values for all flattened states from StateManager
        std::vector<std::any> values;
        values.reserve(flattened_states_.size());

        // Get access to StateManager
        gnc::StateManager* state_manager = dynamic_cast<gnc::StateManager*>(getStateAccess());
        if (!state_manager) {
            LOG_COMPONENT_ERROR("Failed to access StateManager for data collection");
            return;
        }

        for (const auto& flattened_state : flattened_states_) {
            try {
                // Get the raw state value and extract the component
                const std::any& raw_value = state_manager->getRawStateValue(flattened_state.original_state_id);
                double scalar_value = extractScalarFromAny(raw_value, flattened_state.type_name, flattened_state.component_index);
                values.emplace_back(scalar_value);
            } catch (const std::exception& e) {
                LOG_COMPONENT_WARN("Failed to get flattened state {}: {}", 
                                   flattened_state.flattened_name, 
                                   e.what());
                // Push NaN as placeholder for missing data
                values.emplace_back(std::numeric_limits<double>::quiet_NaN());
            }
        }

        // Write data point using file writer
        if (file_writer_) {
            try {
                file_writer_->writeDataPoint(current_time, values);
            } catch (const std::exception& e) {
                LOG_COMPONENT_ERROR("Failed to write data point: {}", e.what());
            }
        }

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
    
    // Use an unordered_set to avoid duplicates during selection
    std::unordered_set<StateId> unique_states;
    
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
            processSpecificStateSelector(selector, all_available_states, unique_states);
        } else if (!selector.component_regex.empty()) {
            // Handle regex-based selector
            processRegexSelector(selector, all_available_states, unique_states);
        }
    }
    
    // Convert set to vector
    states_to_log_.assign(unique_states.begin(), unique_states.end());
    
    LOG_COMPONENT_INFO("State discovery completed, selected {} states for logging", states_to_log_.size());
    
    // Log selected states for debugging
    for (const auto& state_id : states_to_log_) {
        LOG_COMPONENT_DEBUG("Selected state: {}.{}.{}", 
                           state_id.component.vehicleId, 
                           state_id.component.name, 
                           state_id.name);
    }

    // Flatten multi-dimensional states
    flattenStates();
}

void DataLogger::flattenStates() {
    LOG_COMPONENT_DEBUG("Flattening multi-dimensional states");

    flattened_states_.clear();

    // Get access to StateManager
    gnc::StateManager* state_manager = dynamic_cast<gnc::StateManager*>(getStateAccess());
    if (!state_manager) {
        LOG_COMPONENT_ERROR("Failed to access StateManager for state flattening");
        return;
    }

    for (const auto& state_id : states_to_log_) {
        std::string type_name = state_manager->getStateType(state_id);
        
        // Create base name for flattened states
        std::string base_name = state_id.component.name + "." + state_id.name;
        
        if (type_name == typeid(Vector3d).name()) {
            // Flatten Vector3d to _x, _y, _z
            flattened_states_.push_back({state_id, base_name + "_x", type_name, 0});
            flattened_states_.push_back({state_id, base_name + "_y", type_name, 1});
            flattened_states_.push_back({state_id, base_name + "_z", type_name, 2});
            LOG_COMPONENT_DEBUG("Flattened Vector3d state: {} -> {}_x, {}_y, {}_z", 
                               base_name, base_name, base_name, base_name);
        } else if (type_name == typeid(Quaterniond).name()) {
            // Flatten Quaterniond to _w, _x, _y, _z
            flattened_states_.push_back({state_id, base_name + "_w", type_name, 0});
            flattened_states_.push_back({state_id, base_name + "_x", type_name, 1});
            flattened_states_.push_back({state_id, base_name + "_y", type_name, 2});
            flattened_states_.push_back({state_id, base_name + "_z", type_name, 3});
            LOG_COMPONENT_DEBUG("Flattened Quaterniond state: {} -> {}_w, {}_x, {}_y, {}_z", 
                               base_name, base_name, base_name, base_name, base_name);
        } else {
            // Scalar types remain as single entries
            flattened_states_.push_back({state_id, base_name, type_name, 0});
            LOG_COMPONENT_DEBUG("Scalar state: {} (type: {})", base_name, type_name);
        }
    }

    LOG_COMPONENT_INFO("State flattening completed: {} original states -> {} flattened states", 
                       states_to_log_.size(), flattened_states_.size());
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

void DataLogger::processSpecificStateSelector(const StateSelector& selector, const std::vector<StateId>& all_available_states, std::unordered_set<StateId>& unique_states) {
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
                unique_states.insert(target_state_id);
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

void DataLogger::processRegexSelector(const StateSelector& selector, const std::vector<StateId>& all_available_states, std::unordered_set<StateId>& unique_states) {
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
            unique_states.insert(state_id);
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

nlohmann::json DataLogger::collectMetadata() {
    nlohmann::json metadata;
    
    try {
        LOG_COMPONENT_DEBUG("Collecting metadata for DataLogger");
        
        // 1. Add timestamp generation for file creation time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp_ss;
        timestamp_ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        metadata["creation_timestamp"] = timestamp_ss.str();
        LOG_COMPONENT_DEBUG("Added creation timestamp: {}", timestamp_ss.str());
        
        // 2. Add Git hash extraction using system command "git rev-parse HEAD"
        try {
#ifdef _WIN32
            std::string git_command = "git rev-parse HEAD 2>nul";
#else
            std::string git_command = "git rev-parse HEAD 2>/dev/null";
#endif
            
            FILE* pipe = nullptr;
#ifdef _WIN32
            pipe = _popen(git_command.c_str(), "r");
#else
            pipe = popen(git_command.c_str(), "r");
#endif
            
            if (pipe) {
                char buffer[128];
                std::string git_hash;
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    git_hash += buffer;
                }
                
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                
                // Remove trailing newline if present
                if (!git_hash.empty() && git_hash.back() == '\n') {
                    git_hash.pop_back();
                }
                
                // Validate git hash (should be 40 characters for full hash)
                if (!git_hash.empty() && git_hash.length() >= 7 && 
                    git_hash.find("fatal") == std::string::npos &&
                    git_hash.find("not a git repository") == std::string::npos) {
                    metadata["git_hash"] = git_hash;
                    LOG_COMPONENT_DEBUG("Added Git hash: {}", git_hash);
                } else {
                    LOG_COMPONENT_DEBUG("Git hash not available or invalid");
                    metadata["git_hash"] = "not_available";
                }
            } else {
                LOG_COMPONENT_DEBUG("Failed to execute git command");
                metadata["git_hash"] = "not_available";
            }
        } catch (const std::exception& e) {
            LOG_COMPONENT_DEBUG("Could not retrieve Git hash: {}", e.what());
            metadata["git_hash"] = "not_available";
        }
        
        // 3. Serialize current configuration snapshot from ConfigManager
        try {
            auto& config_manager = ConfigManager::getInstance();
            
            // Create a comprehensive configuration snapshot
            nlohmann::json config_snapshot;
            
            // Get all configuration types
            std::vector<ConfigFileType> config_types = {
                ConfigFileType::CORE,
                ConfigFileType::DYNAMICS,
                ConfigFileType::ENVIRONMENT,
                ConfigFileType::EFFECTORS,
                ConfigFileType::LOGIC,
                ConfigFileType::SENSORS,
                ConfigFileType::UTILITY
            };
            
            for (auto config_type : config_types) {
                try {
                    std::string type_name = config_manager.configTypeToString(config_type);
                    auto config = config_manager.getConfig(config_type);
                    if (!config.empty()) {
                        config_snapshot[type_name] = config;
                    }
                } catch (const std::exception& e) {
                    LOG_COMPONENT_DEBUG("Could not get config for type: {}", e.what());
                }
            }
            
            if (!config_snapshot.empty()) {
                metadata["config_snapshot"] = config_snapshot;
                LOG_COMPONENT_DEBUG("Added configuration snapshot with {} config types", config_snapshot.size());
            } else {
                LOG_COMPONENT_DEBUG("Configuration snapshot is empty");
                metadata["config_snapshot"] = "not_available";
            }
            
        } catch (const std::exception& e) {
            LOG_COMPONENT_DEBUG("Could not serialize configuration snapshot: {}", e.what());
            metadata["config_snapshot"] = "not_available";
        }
        
        // 4. Add additional metadata
        metadata["datalogger_version"] = "1.0";
        metadata["framework_version"] = "GNC Meta-Framework";
        
        LOG_COMPONENT_INFO("Metadata collection completed successfully");
        
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error collecting metadata: {}", e.what());
        // Return minimal metadata on error
        metadata["creation_timestamp"] = "error";
        metadata["git_hash"] = "error";
        metadata["config_snapshot"] = "error";
    }
    
    return metadata;
}

std::any DataLogger::getRawStateValue(const gnc::states::StateId& state_id) {
    // Get access to StateManager to use the new getRawStateValue method
    gnc::StateManager* state_manager = dynamic_cast<gnc::StateManager*>(getStateAccess());
    if (!state_manager) {
        throw std::runtime_error("Failed to access StateManager for raw state value retrieval");
    }
    
    try {
        return state_manager->getRawStateValue(state_id);
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error getting raw state value for {}.{}.{}: {}", 
                           state_id.component.vehicleId, 
                           state_id.component.name, 
                           state_id.name, 
                           e.what());
        throw;
    }
}

double DataLogger::extractScalarFromAny(const std::any& value, const std::string& type_name, int component_index) {
    try {
        if (type_name == typeid(double).name()) {
            return std::any_cast<double>(value);
        } else if (type_name == typeid(float).name()) {
            return static_cast<double>(std::any_cast<float>(value));
        } else if (type_name == typeid(int).name()) {
            return static_cast<double>(std::any_cast<int>(value));
        } else if (type_name == typeid(uint64_t).name()) {
            return static_cast<double>(std::any_cast<uint64_t>(value));
        } else if (type_name == typeid(bool).name()) {
            return std::any_cast<bool>(value) ? 1.0 : 0.0;
        } else if (type_name == typeid(Vector3d).name()) {
            const Vector3d& vec = std::any_cast<const Vector3d&>(value);
            switch (component_index) {
                case 0: return vec.x();
                case 1: return vec.y();
                case 2: return vec.z();
                default: 
                    throw std::runtime_error("Invalid component index for Vector3d: " + std::to_string(component_index));
            }
        } else if (type_name == typeid(Quaterniond).name()) {
            const Quaterniond& quat = std::any_cast<const Quaterniond&>(value);
            switch (component_index) {
                case 0: return quat.w();
                case 1: return quat.x();
                case 2: return quat.y();
                case 3: return quat.z();
                default: 
                    throw std::runtime_error("Invalid component index for Quaterniond: " + std::to_string(component_index));
            }
        } else if (type_name == typeid(std::string).name()) {
            // For strings, we can't convert to double meaningfully
            // Return NaN to indicate non-numeric data
            return std::numeric_limits<double>::quiet_NaN();
        } else if (type_name == typeid(std::vector<double>).name()) {
            const std::vector<double>& vec = std::any_cast<const std::vector<double>&>(value);
            if (component_index >= 0 && component_index < static_cast<int>(vec.size())) {
                return vec[component_index];
            } else {
                throw std::runtime_error("Invalid component index for std::vector<double>: " + std::to_string(component_index));
            }
        } else {
            LOG_COMPONENT_WARN("Unsupported type for scalar extraction: {}", type_name);
            return std::numeric_limits<double>::quiet_NaN();
        }
    } catch (const std::bad_any_cast& e) {
        LOG_COMPONENT_ERROR("Failed to cast std::any value: {}", e.what());
        return std::numeric_limits<double>::quiet_NaN();
    } catch (const std::exception& e) {
        LOG_COMPONENT_ERROR("Error extracting scalar from any: {}", e.what());
        return std::numeric_limits<double>::quiet_NaN();
    }
}



} // namespace utility
} // namespace components
} // namespace gnc