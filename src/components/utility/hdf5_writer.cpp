/**
 * @file hdf5_writer.cpp
 * @brief HDF5 file writer implementation
 */

#include "gnc/components/utility/hdf5_writer.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/common/types.hpp"
#include "math/math.hpp"
#include <filesystem>
#include <cstring>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <random>

// --- 新增的跨平台兼容代码 ---
#ifdef _MSC_VER // 如果是 MSVC 编译器
    #include <stdio.h>
    #define popen _popen
    #define pclose _pclose
#endif

// Conditional HDF5 includes
#ifdef HDF5_AVAILABLE
#include <H5Cpp.h>
using namespace H5;
#endif

namespace gnc {
namespace components {
namespace utility {

// PIMPL implementation struct
struct HDF5Writer::HDF5WriterImpl {
#ifdef HDF5_AVAILABLE
    std::unique_ptr<H5File> file;
    std::unique_ptr<Group> data_group;
    std::unique_ptr<DataSet> time_dataset;
    std::unordered_map<std::string, std::unique_ptr<Group>> component_groups;
    std::unordered_map<std::string, std::unique_ptr<DataSet>> state_datasets;
    std::unordered_map<std::string, size_t> state_dimensions;
#endif
    
    HDF5WriterImpl() = default;
    ~HDF5WriterImpl() = default;
};

HDF5Writer::HDF5Writer() 
    : impl_(std::make_unique<HDF5WriterImpl>())
    , initialized_(false)
    , current_row_(0)
{
}

HDF5Writer::~HDF5Writer() {
    if (initialized_) {
        try {
            finalize();
        } catch (const std::exception& e) {
            // Log error but don't throw in destructor
            LOG_ERROR("Error in HDF5Writer destructor: {}", e.what());
        }
    }
}

bool HDF5Writer::isHDF5Available() {
#ifdef HDF5_AVAILABLE
    return true;
#else
    return false;
#endif
}

void HDF5Writer::initialize(const std::string& file_path, 
                           const std::vector<gnc::states::StateId>& states,
                           bool include_metadata,
                           const nlohmann::json& metadata_json) {
#ifndef HDF5_AVAILABLE
    throw std::runtime_error("HDF5 library is not available. Please install HDF5 development libraries and recompile, or use CSV format instead.");
#else
    if (initialized_) {
        throw std::runtime_error("HDF5Writer already initialized");
    }

    if (states.empty()) {
        throw std::runtime_error("Cannot initialize HDF5Writer with empty states list");
    }

    states_ = states;
    metadata_ = metadata_json;
    
    try {
        // Generate unique filename with timestamp
        std::string unique_file_path = generateUniqueFilename(file_path);
        
        // Create output directory if it doesn't exist
        std::filesystem::path file_path_obj(unique_file_path);
        if (file_path_obj.has_parent_path()) {
            std::filesystem::create_directories(file_path_obj.parent_path());
        }

        // Create HDF5 file with unique name
        impl_->file = std::make_unique<H5File>(unique_file_path, H5F_ACC_TRUNC);
        
        LOG_INFO("Created HDF5 file: {}", unique_file_path);
        
        // Write metadata if requested
        if (include_metadata) {
            writeMetadata();
        }
        
        // Create data group
        impl_->data_group = std::make_unique<Group>(impl_->file->createGroup("/data"));
        
        // Create datasets for all states
        createDatasets();
        
        initialized_ = true;
        current_row_ = 0;
        
        LOG_DEBUG("HDF5Writer initialized successfully: {}", file_path);
        
    } catch (const Exception& e) {
        throw std::runtime_error("HDF5 initialization failed: " + std::string(e.getCDetailMsg()));
    } catch (const std::exception& e) {
        throw std::runtime_error("HDF5Writer initialization failed: " + std::string(e.what()));
    }
#endif
}

void HDF5Writer::writeDataPoint(double time, const std::vector<std::any>& values) {
#ifndef HDF5_AVAILABLE
    throw std::runtime_error("HDF5 library is not available");
#else
    if (!initialized_) {
        throw std::runtime_error("HDF5Writer not initialized");
    }
    
    if (values.size() != states_.size()) {
        throw std::runtime_error("Values count (" + std::to_string(values.size()) + 
                                ") does not match states count (" + std::to_string(states_.size()) + ")");
    }
    
    try {
        // Extend all datasets to accommodate new row
        hsize_t new_dims[2] = {current_row_ + 1, 0};
        
        // Extend and write time dataset
        new_dims[1] = 1;
        impl_->time_dataset->extend(new_dims);
        
        // Select hyperslab for time data
        DataSpace time_filespace = impl_->time_dataset->getSpace();
        hsize_t time_offset[2] = {current_row_, 0};
        hsize_t time_count[2] = {1, 1};
        time_filespace.selectHyperslab(H5S_SELECT_SET, time_count, time_offset);
        
        // Create memory space for time
        DataSpace time_memspace(2, time_count);
        
        // Write time data
        impl_->time_dataset->write(&time, PredType::NATIVE_DOUBLE, time_memspace, time_filespace);
        
        // Write state values
        for (size_t i = 0; i < states_.size(); ++i) {
            const auto& state_id = states_[i];
            const auto& value = values[i];
            
            std::string dataset_key = getComponentName(state_id) + "." + getStateName(state_id);
            auto dataset_it = impl_->state_datasets.find(dataset_key);
            if (dataset_it == impl_->state_datasets.end()) {
                LOG_WARN("Dataset not found for state: {}", dataset_key);
                continue;
            }
            
            auto& dataset = dataset_it->second;
            size_t dimensions = impl_->state_dimensions[dataset_key];
            
            // Extend dataset
            new_dims[1] = dimensions;
            dataset->extend(new_dims);
            
            // Select hyperslab for state data
            DataSpace state_filespace = dataset->getSpace();
            hsize_t state_offset[2] = {current_row_, 0};
            hsize_t state_count[2] = {1, dimensions};
            state_filespace.selectHyperslab(H5S_SELECT_SET, state_count, state_offset);
            
            // Create memory space for state
            DataSpace state_memspace(2, state_count);
            
            // Convert value to HDF5 data
            std::vector<double> data_buffer(dimensions);
            size_t written_elements = valueToHDF5Data(value, data_buffer.data());
            
            if (written_elements != dimensions) {
                LOG_WARN("Dimension mismatch for state {}: expected {}, got {}", 
                        dataset_key, dimensions, written_elements);
                // Fill remaining with NaN
                for (size_t j = written_elements; j < dimensions; ++j) {
                    data_buffer[j] = std::numeric_limits<double>::quiet_NaN();
                }
            }
            
            // Write state data
            dataset->write(data_buffer.data(), PredType::NATIVE_DOUBLE, state_memspace, state_filespace);
        }
        
        current_row_++;
        
    } catch (const Exception& e) {
        throw std::runtime_error("HDF5 write failed: " + std::string(e.getCDetailMsg()));
    } catch (const std::exception& e) {
        throw std::runtime_error("HDF5 write failed: " + std::string(e.what()));
    }
#endif
}

void HDF5Writer::finalize() {
#ifndef HDF5_AVAILABLE
    // Nothing to do if HDF5 is not available
    return;
#else
    if (!initialized_) {
        return;
    }
    
    try {
        // Flush all data to disk
        if (impl_->file) {
            impl_->file->flush(H5F_SCOPE_GLOBAL);
        }
        
        // Clean up datasets
        impl_->state_datasets.clear();
        impl_->time_dataset.reset();
        
        // Clean up groups
        impl_->component_groups.clear();
        impl_->data_group.reset();
        
        // Close file
        impl_->file.reset();
        
        initialized_ = false;
        
        LOG_DEBUG("HDF5Writer finalized successfully");
        
    } catch (const Exception& e) {
        LOG_ERROR("HDF5 finalization error: {}", e.getCDetailMsg());
        throw std::runtime_error("HDF5 finalization failed: " + std::string(e.getCDetailMsg()));
    } catch (const std::exception& e) {
        LOG_ERROR("HDF5 finalization error: {}", e.what());
        throw std::runtime_error("HDF5 finalization failed: " + std::string(e.what()));
    }
#endif
}

void HDF5Writer::writeMetadata() {
#ifdef HDF5_AVAILABLE
    try {
        DataSpace scalar_space(H5S_SCALAR);
        
        // Write metadata from collected data
        if (!metadata_.empty()) {
            // Write creation timestamp
            if (metadata_.contains("creation_timestamp")) {
                std::string timestamp = metadata_["creation_timestamp"].get<std::string>();
                StrType str_type(PredType::C_S1, timestamp.length() + 1);
                Attribute timestamp_attr = impl_->file->createAttribute("creation_timestamp", str_type, scalar_space);
                timestamp_attr.write(str_type, timestamp.c_str());
            }
            
            // Write Git hash
            if (metadata_.contains("git_hash")) {
                std::string git_hash = metadata_["git_hash"].get<std::string>();
                if (git_hash != "not_available" && git_hash != "error") {
                    StrType git_str_type(PredType::C_S1, git_hash.length() + 1);
                    Attribute git_attr = impl_->file->createAttribute("git_hash", git_str_type, scalar_space);
                    git_attr.write(git_str_type, git_hash.c_str());
                    LOG_DEBUG("Added Git hash to metadata: {}", git_hash);
                }
            }
            
            // Write framework version
            if (metadata_.contains("framework_version")) {
                std::string framework_version = metadata_["framework_version"].get<std::string>();
                StrType framework_str_type(PredType::C_S1, framework_version.length() + 1);
                Attribute framework_attr = impl_->file->createAttribute("framework_version", framework_str_type, scalar_space);
                framework_attr.write(framework_str_type, framework_version.c_str());
            }
            
            if (metadata_.contains("datalogger_version")) {
                std::string datalogger_version = metadata_["datalogger_version"].get<std::string>();
                StrType datalogger_str_type(PredType::C_S1, datalogger_version.length() + 1);
                Attribute datalogger_attr = impl_->file->createAttribute("datalogger_version", datalogger_str_type, scalar_space);
                datalogger_attr.write(datalogger_str_type, datalogger_version.c_str());
            }
            
            // Write config snapshot (as JSON string)
            if (metadata_.contains("config_snapshot") && metadata_["config_snapshot"].is_object()) {
                std::string config_json = metadata_["config_snapshot"].dump(-1);  // Compact JSON
                StrType config_str_type(PredType::C_S1, config_json.length() + 1);
                Attribute config_attr = impl_->file->createAttribute("config_snapshot", config_str_type, scalar_space);
                config_attr.write(config_str_type, config_json.c_str());
                LOG_DEBUG("Added configuration snapshot to metadata ({} bytes)", config_json.length());
            }
        } else {
            // Fallback to basic timestamp if no metadata provided
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream timestamp_ss;
            timestamp_ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            
            StrType str_type(PredType::C_S1, timestamp_ss.str().length() + 1);
            Attribute timestamp_attr = impl_->file->createAttribute("creation_timestamp", str_type, scalar_space);
            timestamp_attr.write(str_type, timestamp_ss.str().c_str());
            
            std::string no_metadata = "metadata_not_collected";
            StrType no_meta_str_type(PredType::C_S1, no_metadata.length() + 1);
            Attribute no_meta_attr = impl_->file->createAttribute("metadata_status", no_meta_str_type, scalar_space);
            no_meta_attr.write(no_meta_str_type, no_metadata.c_str());
        }
        
        LOG_DEBUG("Metadata written to HDF5 file");
        
    } catch (const Exception& e) {
        LOG_WARN("Failed to write metadata: {}", e.getCDetailMsg());
    } catch (const std::exception& e) {
        LOG_WARN("Failed to write metadata: {}", e.what());
    }
#endif
}

void HDF5Writer::createDatasets() {
#ifdef HDF5_AVAILABLE
    try {
        // Create time dataset (extensible, 1D)
        hsize_t time_dims[2] = {0, 1};  // Start with 0 rows, 1 column
        hsize_t time_max_dims[2] = {H5S_UNLIMITED, 1};
        DataSpace time_space(2, time_dims, time_max_dims);
        
        // Set up chunking for extensible dataset
        DSetCreatPropList time_plist;
        hsize_t time_chunk_dims[2] = {1000, 1};  // Chunk size for efficient I/O
        time_plist.setChunk(2, time_chunk_dims);
        time_plist.setDeflate(6);  // Enable compression
        
        impl_->time_dataset = std::make_unique<DataSet>(
            impl_->data_group->createDataSet("time", PredType::NATIVE_DOUBLE, time_space, time_plist)
        );
        
        // Group states by component
        std::unordered_map<std::string, std::vector<std::pair<std::string, size_t>>> component_states;
        
        for (const auto& state_id : states_) {
            std::string component_name = getComponentName(state_id);
            std::string state_name = getStateName(state_id);
            
            // Determine dimensions by creating a dummy value
            // For now, assume all states are scalars (dimension 1)
            // This will be refined when we have actual values in writeDataPoint
            size_t dimensions = 1;  // Default to scalar
            
            component_states[component_name].emplace_back(state_name, dimensions);
            impl_->state_dimensions[component_name + "." + state_name] = dimensions;
        }
        
        // Create component groups and datasets
        for (const auto& [component_name, states_info] : component_states) {
            // Create component group if it doesn't exist
            if (impl_->component_groups.find(component_name) == impl_->component_groups.end()) {
                impl_->component_groups[component_name] = std::make_unique<Group>(
                    impl_->data_group->createGroup(component_name)
                );
            }
            
            auto& component_group = impl_->component_groups[component_name];
            
            // Create datasets for each state in this component
            for (const auto& [state_name, dimensions] : states_info) {
                hsize_t state_dims[2] = {0, dimensions};
                hsize_t state_max_dims[2] = {H5S_UNLIMITED, dimensions};
                DataSpace state_space(2, state_dims, state_max_dims);
                
                // Set up chunking
                DSetCreatPropList state_plist;
                hsize_t state_chunk_dims[2] = {1000, dimensions};
                state_plist.setChunk(2, state_chunk_dims);
                state_plist.setDeflate(6);  // Enable compression
                
                std::string dataset_key = component_name + "." + state_name;
                impl_->state_datasets[dataset_key] = std::make_unique<DataSet>(
                    component_group->createDataSet(state_name, PredType::NATIVE_DOUBLE, state_space, state_plist)
                );
            }
        }
        
        LOG_DEBUG("Created {} datasets for {} states", impl_->state_datasets.size(), states_.size());
        
    } catch (const Exception& e) {
        throw std::runtime_error("Failed to create HDF5 datasets: " + std::string(e.getCDetailMsg()));
    }
#endif
}

size_t HDF5Writer::getValueDimensions(const std::any& value) {
    try {
        // Try different types to determine dimensions
        if (value.type() == typeid(double) || value.type() == typeid(float) || 
            value.type() == typeid(int) || value.type() == typeid(bool)) {
            return 1;  // Scalar
        } else if (value.type() == typeid(Vector3d)) {
            return 3;  // 3D vector
        } else if (value.type() == typeid(Quaterniond)) {
            return 4;  // Quaternion (w, x, y, z)
        } else if (value.type() == typeid(std::string)) {
            return 1;  // String as single value (will be converted to double as NaN)
        } else {
            LOG_WARN("Unknown value type, defaulting to scalar");
            return 1;
        }
    } catch (const std::bad_any_cast& e) {
        LOG_WARN("Failed to determine value dimensions: {}", e.what());
        return 1;
    }
}

size_t HDF5Writer::valueToHDF5Data(const std::any& value, double* buffer) {
    try {
        if (value.type() == typeid(double)) {
            buffer[0] = std::any_cast<double>(value);
            return 1;
        } else if (value.type() == typeid(float)) {
            buffer[0] = static_cast<double>(std::any_cast<float>(value));
            return 1;
        } else if (value.type() == typeid(int)) {
            buffer[0] = static_cast<double>(std::any_cast<int>(value));
            return 1;
        } else if (value.type() == typeid(bool)) {
            buffer[0] = std::any_cast<bool>(value) ? 1.0 : 0.0;
            return 1;
        } else if (value.type() == typeid(Vector3d)) {
            const auto& vec = std::any_cast<Vector3d>(value);
            buffer[0] = vec.x();
            buffer[1] = vec.y();
            buffer[2] = vec.z();
            return 3;
        } else if (value.type() == typeid(Quaterniond)) {
            const auto& quat = std::any_cast<Quaterniond>(value);
            buffer[0] = quat.w();
            buffer[1] = quat.x();
            buffer[2] = quat.y();
            buffer[3] = quat.z();
            return 4;
        } else if (value.type() == typeid(std::string)) {
            // Convert string to NaN (indicates non-numeric data)
            buffer[0] = std::numeric_limits<double>::quiet_NaN();
            return 1;
        } else {
            LOG_WARN("Unknown value type, storing as NaN");
            buffer[0] = std::numeric_limits<double>::quiet_NaN();
            return 1;
        }
    } catch (const std::bad_any_cast& e) {
        LOG_WARN("Failed to convert value to HDF5 data: {}", e.what());
        buffer[0] = std::numeric_limits<double>::quiet_NaN();
        return 1;
    }
}

std::string HDF5Writer::getComponentName(const gnc::states::StateId& state_id) {
    // StateId format is typically "VehicleId.ComponentName.StateName"
    // Extract component name - just return the component name field directly
    return state_id.component.name;
}

std::string HDF5Writer::getStateName(const gnc::states::StateId& state_id) {
    // StateId format is typically "VehicleId.ComponentName.StateName"
    // Extract state name - just return the name field directly
    return state_id.name;
}

std::string HDF5Writer::generateUniqueFilename(const std::string& base_path) {
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
        std::string git_command = "git rev-parse --short HEAD 2>/dev/null";
        FILE* pipe = popen(git_command.c_str(), "r");
        if (pipe) {
            char buffer[32];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                run_id = buffer;
                // Remove trailing newline
                if (!run_id.empty() && run_id.back() == '\n') {
                    run_id.pop_back();
                }
            }
            pclose(pipe);
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