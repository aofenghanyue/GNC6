/**
 * @file csv_writer.cpp
 * @brief CSV file writer implementation
 */

#include "gnc/components/utility/csv_writer.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "math/math.hpp"
#include <filesystem>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <chrono>

namespace gnc {
namespace components {
namespace utility {

void CSVWriter::initialize(const std::string& file_path, 
                          const std::vector<gnc::states::StateId>& states,
                          bool include_metadata) {
    if (initialized_) {
        throw std::runtime_error("CSVWriter already initialized");
    }

    try {
        // Store states for later use
        states_ = states;

        // Generate unique filename with timestamp
        std::string unique_file_path = generateUniqueFilename(file_path);

        // Create directory if it doesn't exist
        std::filesystem::path path(unique_file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        // Open file for writing
        file_stream_.open(unique_file_path, std::ios::out | std::ios::trunc);
        if (!file_stream_.is_open()) {
            throw std::runtime_error("Failed to open CSV file: " + unique_file_path);
        }

        LOG_INFO("Created CSV file: {}", unique_file_path);

        // Set precision for floating point numbers
        file_stream_ << std::fixed << std::setprecision(6);

        // Write metadata if requested
        if (include_metadata) {
            writeMetadata();
        }

        initialized_ = true;
        header_written_ = false;

        LOG_DEBUG("CSVWriter initialized successfully");

    } catch (const std::exception& e) {
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        throw std::runtime_error("Failed to initialize CSVWriter: " + std::string(e.what()));
    }
}

void CSVWriter::writeDataPoint(double time, const std::vector<std::any>& values) {
    if (!initialized_) {
        throw std::runtime_error("CSVWriter not initialized");
    }

    if (values.size() != states_.size()) {
        throw std::runtime_error("Values count (" + std::to_string(values.size()) + 
                                ") does not match states count (" + std::to_string(states_.size()) + ")");
    }

    try {
        // Write header on first data point (we need actual values to determine column structure)
        if (!header_written_) {
            writeHeader();
            header_written_ = true;
        }

        // Write time column
        file_stream_ << time;

        // Write all state values
        for (size_t i = 0; i < values.size(); ++i) {
            auto csv_strings = valueToCSVStrings(values[i]);
            for (const auto& csv_str : csv_strings) {
                file_stream_ << "," << csv_str;
            }
        }

        file_stream_ << "\n";

        // Flush periodically to ensure data is written
        static int write_count = 0;
        if (++write_count % 100 == 0) {
            file_stream_.flush();
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to write data point: " + std::string(e.what()));
    }
}

void CSVWriter::finalize() {
    if (!initialized_) {
        return;
    }

    try {
        if (file_stream_.is_open()) {
            file_stream_.flush();
            file_stream_.close();
        }

        initialized_ = false;
        header_written_ = false;

        LOG_DEBUG("CSVWriter finalized successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Error finalizing CSVWriter: {}", e.what());
    }
}

void CSVWriter::writeMetadata() {
    // Write creation timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    file_stream_ << "# creation_timestamp: " 
                << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "\n";

    // Try to get Git hash
    try {
        std::string git_command = "git rev-parse HEAD 2>nul";
        FILE* pipe = _popen(git_command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string git_hash;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                git_hash += buffer;
            }
            _pclose(pipe);
            
            // Remove newline if present
            if (!git_hash.empty() && git_hash.back() == '\n') {
                git_hash.pop_back();
            }
            
            if (!git_hash.empty() && git_hash.find("fatal") == std::string::npos) {
                file_stream_ << "# git_hash: " << git_hash << "\n";
            }
        }
    } catch (const std::exception& e) {
        LOG_DEBUG("Could not retrieve Git hash: {}", e.what());
    }

    // TODO: Add config snapshot when metadata collection is implemented (Task 7)
    file_stream_ << "# config_snapshot: [not yet implemented]\n";
    
    file_stream_ << "#\n";  // Empty comment line for separation
}

void CSVWriter::writeHeader() {
    // Start with time column
    file_stream_ << "time";

    // Add columns for each state
    for (size_t i = 0; i < states_.size(); ++i) {
        const auto& state_id = states_[i];
        
        // For header, we need to determine the column structure
        // Since we don't have the actual values yet, we'll use a simple naming scheme
        // and handle expansion during data writing
        
        std::string base_name = state_id.component.name + "." + state_id.name;
        
        // We'll write a single column name here and expand during data writing if needed
        // This is a simplified approach - in a more robust implementation, we might
        // want to analyze the first data point to determine the exact column structure
        file_stream_ << "," << base_name;
    }

    file_stream_ << "\n";
}

std::string CSVWriter::valueToCSVString(const std::any& value) {
    auto csv_strings = valueToCSVStrings(value);
    if (csv_strings.empty()) {
        return "";
    }
    
    // If multiple values, join with semicolon (this shouldn't happen in normal CSV)
    std::string result = csv_strings[0];
    for (size_t i = 1; i < csv_strings.size(); ++i) {
        result += ";" + csv_strings[i];
    }
    return result;
}

std::vector<std::string> CSVWriter::getColumnNames(const gnc::states::StateId& state_id, const std::any& value) {
    std::vector<std::string> names;
    std::string base_name = state_id.component.name + "." + state_id.name;

    try {
        // Check for Vector3d
        if (value.type() == typeid(Vector3d)) {
            names.push_back(base_name + ".x");
            names.push_back(base_name + ".y");
            names.push_back(base_name + ".z");
            return names;
        }
        
        // Check for Quaterniond
        if (value.type() == typeid(Quaterniond)) {
            names.push_back(base_name + ".w");
            names.push_back(base_name + ".x");
            names.push_back(base_name + ".y");
            names.push_back(base_name + ".z");
            return names;
        }
        
        // Check for Vector4d
        if (value.type() == typeid(Vector4d)) {
            names.push_back(base_name + ".x");
            names.push_back(base_name + ".y");
            names.push_back(base_name + ".z");
            names.push_back(base_name + ".w");
            return names;
        }
        
    } catch (const std::bad_any_cast& e) {
        LOG_DEBUG("Error determining column names for state {}: {}", base_name, e.what());
    }

    // Default: single column
    names.push_back(base_name);
    return names;
}

std::vector<std::string> CSVWriter::valueToCSVStrings(const std::any& value) {
    std::vector<std::string> result;

    try {
        // Handle double
        if (value.type() == typeid(double)) {
            double val = std::any_cast<double>(value);
            result.push_back(std::to_string(val));
            return result;
        }
        
        // Handle float
        if (value.type() == typeid(float)) {
            float val = std::any_cast<float>(value);
            result.push_back(std::to_string(val));
            return result;
        }
        
        // Handle int
        if (value.type() == typeid(int)) {
            int val = std::any_cast<int>(value);
            result.push_back(std::to_string(val));
            return result;
        }
        
        // Handle bool
        if (value.type() == typeid(bool)) {
            bool val = std::any_cast<bool>(value);
            result.push_back(val ? "1" : "0");
            return result;
        }
        
        // Handle string
        if (value.type() == typeid(std::string)) {
            std::string val = std::any_cast<std::string>(value);
            // Escape commas and quotes in CSV
            if (val.find(',') != std::string::npos || val.find('"') != std::string::npos) {
                // Replace quotes with double quotes and wrap in quotes
                std::string escaped = "\"";
                for (char c : val) {
                    if (c == '"') {
                        escaped += "\"\"";
                    } else {
                        escaped += c;
                    }
                }
                escaped += "\"";
                result.push_back(escaped);
            } else {
                result.push_back(val);
            }
            return result;
        }
        
        // Handle Vector3d
        if (value.type() == typeid(Vector3d)) {
            Vector3d vec = std::any_cast<Vector3d>(value);
            result.push_back(std::to_string(vec.x()));
            result.push_back(std::to_string(vec.y()));
            result.push_back(std::to_string(vec.z()));
            return result;
        }
        
        // Handle Quaterniond
        if (value.type() == typeid(Quaterniond)) {
            Quaterniond quat = std::any_cast<Quaterniond>(value);
            result.push_back(std::to_string(quat.w()));
            result.push_back(std::to_string(quat.x()));
            result.push_back(std::to_string(quat.y()));
            result.push_back(std::to_string(quat.z()));
            return result;
        }
        
        // Handle Vector4d
        if (value.type() == typeid(Vector4d)) {
            Vector4d vec = std::any_cast<Vector4d>(value);
            result.push_back(std::to_string(vec(0)));
            result.push_back(std::to_string(vec(1)));
            result.push_back(std::to_string(vec(2)));
            result.push_back(std::to_string(vec(3)));
            return result;
        }
        
    } catch (const std::bad_any_cast& e) {
        LOG_DEBUG("Error converting value to CSV string: {}", e.what());
    }

    // Fallback: try to convert to string representation
    try {
        std::string type_name = value.type().name();
        result.push_back("[" + type_name + "]");
        LOG_DEBUG("Unknown type for CSV conversion: {}", type_name);
    } catch (const std::exception& e) {
        result.push_back("[unknown]");
        LOG_DEBUG("Failed to convert unknown type to CSV: {}", e.what());
    }

    return result;
}

std::string CSVWriter::generateUniqueFilename(const std::string& base_path) {
    std::filesystem::path path_obj(base_path);
    std::string directory = path_obj.parent_path().string();
    std::string filename = path_obj.stem().string();
    std::string extension = path_obj.extension().string();
    
    // Generate timestamp string (YYYYMMDD_HHMMSS format)
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream timestamp_ss;
    timestamp_ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    // Add milliseconds for extra uniqueness
    timestamp_ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    // Try to get a short Git hash for run identification
    std::string run_id;
    try {
        std::string git_command = "git rev-parse --short HEAD 2>nul";
        FILE* pipe = _popen(git_command.c_str(), "r");
        if (pipe) {
            char buffer[32];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                run_id = buffer;
                // Remove trailing newline
                if (!run_id.empty() && run_id.back() == '\n') {
                    run_id.pop_back();
                }
            }
            _pclose(pipe);
        }
    } catch (const std::exception& e) {
        // If Git fails, use a simple counter or random identifier
        run_id = "run";
    }
    
    // If no Git hash, generate a simple random identifier
    if (run_id.empty()) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        std::stringstream rand_ss;
        rand_ss << std::hex << (std::rand() % 0xFFFF);
        run_id = rand_ss.str();
    }
    
    // Construct unique filename
    std::stringstream unique_filename_ss;
    if (!directory.empty()) {
        unique_filename_ss << directory << "/";
    }
    unique_filename_ss << filename << "_" << timestamp_ss.str() << "_" << run_id << extension;
    
    return unique_filename_ss.str();
}

} // namespace utility
} // namespace components
} // namespace gnc