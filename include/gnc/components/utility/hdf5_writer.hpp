/**
 * @file hdf5_writer.hpp
 * @brief HDF5 file writer implementation for DataLogger
 */

#pragma once

#include "data_logger.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

// Forward declarations to avoid requiring HDF5 headers when HDF5 is not available
namespace H5 {
    class H5File;
    class Group;
    class DataSet;
    class DataSpace;
    class DataType;
}

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief HDF5 file writer implementation
 * 
 * @details HDF5Writer handles writing simulation data to HDF5 files with:
 * - Hierarchical structure with root attributes and data groups
 * - Extensible datasets for efficient data appending
 * - Vector/quaternion data stored as multi-dimensional arrays [N,D]
 * - Built-in compression for storage efficiency
 * - Metadata integration (Git hash, config snapshot, timestamps)
 */
class HDF5Writer : public FileWriter {
public:
    /**
     * @brief Constructor
     */
    HDF5Writer();

    /**
     * @brief Destructor
     */
    virtual ~HDF5Writer();

    /**
     * @brief Initialize the HDF5 file writer
     * @param file_path Path to the output HDF5 file
     * @param states List of states that will be recorded
     * @param include_metadata Whether to include metadata in the file
     * @param metadata_json JSON object containing metadata (optional)
     * @throws std::runtime_error if initialization fails or HDF5 is not available
     */
    void initialize(const std::string& file_path, 
                   const std::vector<gnc::states::StateId>& states,
                   bool include_metadata,
                   const nlohmann::json& metadata_json = nlohmann::json()) override;

    /**
     * @brief Write a single data point to the HDF5 file
     * @param time Current simulation time
     * @param values Vector of state values corresponding to the states list from initialize()
     * @throws std::runtime_error if write operation fails
     */
    void writeDataPoint(double time, 
                       const std::vector<std::any>& values) override;

    /**
     * @brief Finalize and close the HDF5 file
     * @details Flushes any remaining data and properly closes file handles
     * @throws std::runtime_error if finalization fails
     */
    void finalize() override;

    /**
     * @brief Check if HDF5 library is available at runtime
     * @return true if HDF5 library is available and can be used
     */
    static bool isHDF5Available();

private:
    // Forward declarations for implementation details
    struct HDF5WriterImpl;
    std::unique_ptr<HDF5WriterImpl> impl_;  ///< PIMPL idiom to hide HDF5 dependencies

    bool initialized_;                      ///< Whether writer has been initialized
    std::vector<gnc::states::StateId> states_;  ///< Cached states list
    size_t current_row_;                    ///< Current row index for data writing
    nlohmann::json metadata_;              ///< Cached metadata for writing
    
    /**
     * @brief Write metadata as root attributes
     */
    void writeMetadata();

    /**
     * @brief Create extensible datasets for all states
     */
    void createDatasets();

    /**
     * @brief Get the dimensions for a state value (1 for scalar, N for vector)
     * @param value Sample value to determine dimensions
     * @return Number of dimensions
     */
    size_t getValueDimensions(const std::any& value);

    /**
     * @brief Convert state value to HDF5-compatible data
     * @param value The state value to convert
     * @param buffer Output buffer for the converted data
     * @return Number of elements written to buffer
     */
    size_t valueToHDF5Data(const std::any& value, double* buffer);

    /**
     * @brief Get component name from StateId
     * @param state_id The state identifier
     * @return Component name for grouping
     */
    std::string getComponentName(const gnc::states::StateId& state_id);

    /**
     * @brief Get state name from StateId (without component prefix)
     * @param state_id The state identifier
     * @return State name for dataset naming
     */
    std::string getStateName(const gnc::states::StateId& state_id);

    /**
     * @brief Generate unique filename with timestamp and optional run identifier
     * @param base_path Base file path (e.g., "logs/simulation_data.h5")
     * @return Unique file path with timestamp (e.g., "logs/simulation_data_20250719_205913_abc123.h5")
     */
    std::string generateUniqueFilename(const std::string& base_path);
};

} // namespace utility
} // namespace components
} // namespace gnc