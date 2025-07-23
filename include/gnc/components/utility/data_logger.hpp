/**
 * @file data_logger.hpp
 * @brief DataLogger component for recording simulation data
 * 
 * @details DataLogger Component Design
 * 
 * 1. Core Functionality
 *    - State Discovery: Automatically discover and select states based on regex patterns
 *    - Data Recording: Record selected states to HDF5 or CSV files
 *    - Metadata Integration: Include Git hash, configuration snapshots, and timestamps
 *    - Flexible Configuration: Configure through YAML files
 * 
 * 2. Design Patterns
 *    - Observer Pattern: Passively observes system states without modification
 *    - Strategy Pattern: Different file writers for different output formats
 *    - Component Pattern: Inherits from ComponentBase for framework integration
 * 
 * 3. Usage Example
 *    @code
 *    // Configuration in utility.yaml:
 *    DataLogger:
 *      format: "hdf5"
 *      file_path: "logs/simulation_data.h5"
 *      log_frequency_hz: 100
 *      log_metadata: true
 *      selectors:
 *        - component_regex: ".*Guidance.*"
 *          state_regex: ".*"
 *    @endcode
 */
#pragma once

#include "../../core/component_base.hpp"
#include "../../common/types.hpp"
#include "gnc/core/component_registrar.hpp"
#include <string>
#include <vector>
#include <memory>
#include <any>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief Abstract interface for file writers
 * 
 * @details FileWriter provides a common interface for different output formats.
 * This allows the DataLogger to support multiple file formats (HDF5, CSV) 
 * through a unified interface using the Strategy pattern.
 */
class FileWriter {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~FileWriter() = default;

    /**
     * @brief Initialize the file writer
     * @param file_path Path to the output file
     * @param states List of states that will be recorded
     * @param include_metadata Whether to include metadata in the file
     * @param metadata_json JSON object containing metadata (optional)
     * @throws std::runtime_error if initialization fails
     */
    virtual void initialize(const std::string& file_path, 
                          const std::vector<gnc::states::StateId>& states,
                          bool include_metadata,
                          const nlohmann::json& metadata_json = nlohmann::json()) = 0;

    /**
     * @brief Write a single data point to the file
     * @param time Current simulation time
     * @param values Vector of state values corresponding to the states list from initialize()
     * @throws std::runtime_error if write operation fails
     */
    virtual void writeDataPoint(double time, 
                               const std::vector<std::any>& values) = 0;

    /**
     * @brief Finalize and close the file
     * @details Flushes any remaining data and properly closes file handles
     * @throws std::runtime_error if finalization fails
     */
    virtual void finalize() = 0;
};

/**
 * @brief Factory function to create appropriate file writer based on format
 * @param format Output format ("hdf5" or "csv")
 * @return Unique pointer to the created file writer
 * @throws std::invalid_argument if format is not supported
 */
std::unique_ptr<FileWriter> createFileWriter(const std::string& format);

/**
 * @brief Configuration structure for state selectors
 */
struct StateSelector {
    std::string state;                    ///< Specific state name (format: "Component.state")
    std::string component_regex;          ///< Component name regex pattern
    std::string state_regex;              ///< State name regex pattern
    std::string exclude_state_regex;      ///< Exclusion regex pattern
    
    StateSelector() = default;
    
    StateSelector(const std::string& specific_state) 
        : state(specific_state) {}
        
    StateSelector(const std::string& comp_regex, const std::string& st_regex, 
                  const std::string& exclude_regex = "")
        : component_regex(comp_regex), state_regex(st_regex), exclude_state_regex(exclude_regex) {}
};

/**
 * @brief DataLogger component for recording simulation data
 * 
 * @details The DataLogger component is designed as a passive observer that:
 * 1. Discovers states based on configurable regex patterns
 * 2. Records selected states to files (HDF5 or CSV format)
 * 3. Includes metadata for reproducible scientific experiments
 * 4. Operates efficiently without impacting simulation performance
 * 
 * Key Features:
 * - Non-intrusive: No modifications needed to existing components
 * - Flexible: Pattern-based state selection
 * - Efficient: Cached state lists and optimized I/O
 * - Scientific: Built-in metadata for reproducibility
 */
class DataLogger : public gnc::states::ComponentBase {
public:
    /**
     * @brief Constructor with custom instance name
     * @param id Vehicle ID this component belongs to
     * @param instanceName Custom name for this component instance
     */
    DataLogger(gnc::states::VehicleId id, const std::string& instanceName="")
        : ComponentBase(id, "DataLogger", instanceName)
        , output_format_("hdf5")
        , file_path_("logs/datalogger_output.h5")
        , log_frequency_hz_(0.0)
        , log_metadata_(true)
        , last_log_time_(0.0)
        , initialized_(false)
    {
        LOG_COMPONENT_DEBUG("DataLogger created with instance name: {}", instanceName);
    }
    /**
     * @brief Destructor
     */
    virtual ~DataLogger() = default;

    // Disable copy and move operations
    DataLogger(const DataLogger&) = delete;
    DataLogger& operator=(const DataLogger&) = delete;
    DataLogger(DataLogger&&) = delete;
    DataLogger& operator=(DataLogger&&) = delete;

    /**
     * @brief Get component type identifier
     * @return Component type string for factory registration
     */
    std::string getComponentType() const override;

    /**
     * @brief Initialize the DataLogger component
     * 
     * @details Initialization process:
     * 1. Load configuration from utility.yaml
     * 2. Discover and select states based on regex patterns
     * 3. Create and initialize appropriate file writer
     * 4. Set up metadata collection if enabled
     * 
     * Called once after all components are registered but before first update.
     */
    void initialize() override;

    /**
     * @brief Finalize the DataLogger component
     * 
     * @details Finalization process:
     * 1. Flush all data buffers to disk
     * 2. Properly close file handles
     * 3. Clean up resources
     * 4. Ensure data integrity
     * 
     * Called when simulation ends or component is destroyed.
     */
    void finalize() override;

protected:
    /**
     * @brief Update implementation - records data at configured frequency
     * 
     * @details Update process:
     * 1. Check if it's time to log based on frequency setting
     * 2. Get current time from TimingManager
     * 3. Collect values for all cached states
     * 4. Write data point using file writer
     * 
     * Called every simulation step by the StateManager.
     */
    void updateImpl() override;

private:
    // Configuration parameters (loaded from utility.yaml)
    std::string output_format_;        ///< Output format: "hdf5" or "csv"
    std::string file_path_;           ///< Output file path
    double log_frequency_hz_;         ///< Logging frequency in Hz (0 = every step)
    bool log_metadata_;               ///< Whether to include metadata

    // Configuration selectors
    std::vector<StateSelector> selectors_;  ///< State selection rules from configuration
    
    // Runtime state
    std::vector<gnc::states::StateId> states_to_log_;  ///< Cached list of states to record
    std::unique_ptr<FileWriter> file_writer_;          ///< File writer instance
    double last_log_time_;            ///< Last time data was logged
    bool initialized_;                ///< Whether component has been initialized

    /**
     * @brief Structure to hold flattened state information
     */
    struct FlattenedState {
        gnc::states::StateId original_state_id;  ///< Original state ID
        std::string flattened_name;              ///< Flattened name (e.g., "position_x", "attitude_w")
        std::string type_name;                   ///< Type name from RTTI
        int component_index;                     ///< Index within the original state (0 for scalars)
    };

    std::vector<FlattenedState> flattened_states_;     ///< List of flattened states for logging

    /**
     * @brief Load configuration from utility.yaml
     * @throws std::runtime_error if configuration is invalid
     */
    void loadConfiguration();

    /**
     * @brief Discover and select states based on configuration
     * @details Uses regex patterns to match component and state names
     */
    void discoverAndSelectStates();

    /**
     * @brief Flatten multi-dimensional states into scalar components
     * @details Converts Vector3d to _x, _y, _z and Quaterniond to _w, _x, _y, _z
     */
    void flattenStates();

    /**
     * @brief Check if it's time to log data based on frequency setting
     * @param current_time Current simulation time
     * @return true if data should be logged
     */
    bool shouldLog(double current_time) const;

    /**
     * @brief Process specific state selector (e.g., "Component.state")
     * @param selector State selector configuration
     * @param all_available_states List of all available states from StateManager
     */
    void processSpecificStateSelector(const StateSelector& selector, const std::vector<gnc::states::StateId>& all_available_states, std::unordered_set<gnc::states::StateId>& unique_states);

    /**
     * @brief Process regex-based state selector
     * @param selector State selector configuration with regex patterns
     * @param all_available_states List of all available states from StateManager
     * @param unique_states Set to store unique selected states
     */
    void processRegexSelector(const StateSelector& selector, const std::vector<gnc::states::StateId>& all_available_states, std::unordered_set<gnc::states::StateId>& unique_states);

    /**
     * @brief Collect metadata for logging
     * @return JSON object containing metadata (git hash, config snapshot, timestamp)
     */
    nlohmann::json collectMetadata();

    /**
     * @brief Get raw state value as std::any
     * @param state_id State identifier
     * @return State value as std::any
     * @throws std::runtime_error if state cannot be accessed
     */
    std::any getRawStateValue(const gnc::states::StateId& state_id);

    /**
     * @brief Extract scalar value from std::any based on type and component index
     * @param value The std::any value containing the state data
     * @param type_name The type name from RTTI
     * @param component_index The index of the component to extract (0 for scalars)
     * @return Extracted scalar value as double
     * @throws std::runtime_error if extraction fails
     */
    double extractScalarFromAny(const std::any& value, const std::string& type_name, int component_index);
};
// Register the DataLogger component with the factory
static gnc::ComponentRegistrar<DataLogger> data_logger_registrar("DataLogger");
} // namespace utility
} // namespace components
} // namespace gnc