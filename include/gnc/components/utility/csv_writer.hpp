/**
 * @file csv_writer.hpp
 * @brief CSV file writer implementation for DataLogger
 */

#pragma once

#include "data_logger.hpp"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief CSV file writer implementation
 * 
 * @details CSVWriter handles writing simulation data to CSV files with:
 * - Metadata as comments at the beginning of the file
 * - Header row with time and all selected state names
 * - Proper CSV formatting with comma separation
 * - Vector/quaternion data expanded to multiple columns
 */
class CSVWriter : public FileWriter {
public:
    /**
     * @brief Constructor
     */
    CSVWriter() = default;

    /**
     * @brief Destructor
     */
    virtual ~CSVWriter() = default;

    /**
     * @brief Initialize the CSV file writer
     * @param file_path Path to the output CSV file
     * @param states List of states that will be recorded
     * @param include_metadata Whether to include metadata in the file
     * @param metadata_json JSON object containing metadata (optional)
     * @throws std::runtime_error if initialization fails
     */
    void initialize(const std::string& file_path, 
                   const std::vector<gnc::states::StateId>& states,
                   bool include_metadata,
                   const nlohmann::json& metadata_json = nlohmann::json()) override;

    /**
     * @brief Write a single data point to the CSV file
     * @param time Current simulation time
     * @param values Vector of state values corresponding to the states list from initialize()
     * @throws std::runtime_error if write operation fails
     */
    void writeDataPoint(double time, 
                       const std::vector<std::any>& values) override;

    /**
     * @brief Finalize and close the CSV file
     * @details Flushes any remaining data and properly closes file handles
     * @throws std::runtime_error if finalization fails
     */
    void finalize() override;

private:
    std::ofstream file_stream_;                           ///< Output file stream
    std::vector<gnc::states::StateId> states_;           ///< Cached states list
    bool initialized_;                                    ///< Whether writer has been initialized
    bool header_written_;                                ///< Whether header row has been written
    nlohmann::json metadata_;                            ///< Cached metadata for writing

    /**
     * @brief Write metadata as comments at the beginning of the file
     */
    void writeMetadata();

    /**
     * @brief Write the header row with column names
     */
    void writeHeader();

    /**
     * @brief Convert a state value to CSV string representation
     * @param value The state value to convert
     * @return String representation for CSV
     */
    std::string valueToCSVString(const std::any& value);

    /**
     * @brief Get column names for a state (handles vector/quaternion expansion)
     * @param state_id The state identifier
     * @param value Sample value to determine type and dimensions
     * @return Vector of column names
     */
    std::vector<std::string> getColumnNames(const gnc::states::StateId& state_id, const std::any& value);

    /**
     * @brief Convert a value to multiple CSV strings (for vector/quaternion expansion)
     * @param value The value to convert
     * @return Vector of string values for CSV columns
     */
    std::vector<std::string> valueToCSVStrings(const std::any& value);

    /**
     * @brief Generate unique filename with timestamp and optional run identifier
     * @param base_path Base file path (e.g., "logs/simulation_data.csv")
     * @return Unique file path with timestamp (e.g., "logs/simulation_data_20250719_205913_abc123.csv")
     */
    std::string generateUniqueFilename(const std::string& base_path);
};

} // namespace utility
} // namespace components
} // namespace gnc