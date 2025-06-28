/**
 * @file config_manager.cpp
 * @brief GNC 框架配置管理器实现
 */

#include "gnc/components/utility/config_manager.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace gnc {
namespace components {
namespace utility {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfigs(const std::string& config_dir_path) {
    config_dir_path_ = config_dir_path;
    
    // 确保配置目录存在
    if (!std::filesystem::exists(config_dir_path)) {
        std::filesystem::create_directories(config_dir_path);
    }
    
    bool all_success = true;
    
    // 加载所有配置文件
    std::vector<ConfigFileType> types = {
        ConfigFileType::CORE,
        ConfigFileType::DYNAMICS,
        ConfigFileType::ENVIRONMENT,
        ConfigFileType::EFFECTORS,
        ConfigFileType::LOGIC,
        ConfigFileType::SENSORS,
        ConfigFileType::UTILITY
    };
    
    for (auto type : types) {
        std::string filename = configTypeToString(type) + ".json";
        std::string filepath = config_dir_path + "/" + filename;
        
        if (!loadConfig(type, filepath)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool ConfigManager::loadConfig(ConfigFileType type, const std::string& config_file_path) {
    try {
        config_file_paths_[type] = config_file_path;
        
        if (!std::filesystem::exists(config_file_path)) {
            // 如果文件不存在，创建默认配置文件
            auto default_config = getDefaultConfig(type);
            std::ofstream file(config_file_path);
            if (file.is_open()) {
                file << default_config.dump(4);
                file.close();
                configs_[type] = default_config;
                return true;
            } else {
                std::cerr << "Failed to create default config file: " << config_file_path << std::endl;
                configs_[type] = default_config;
                return false;
            }
        }
        
        std::ifstream file(config_file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << config_file_path << std::endl;
            configs_[type] = getDefaultConfig(type);
            return false;
        }
        
        nlohmann::json config;
        file >> config;
        file.close();
        
        // 合并默认配置和加载的配置
        auto default_config = getDefaultConfig(type);
        configs_[type] = mergeConfigs(default_config, config);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config file " << config_file_path << ": " << e.what() << std::endl;
        configs_[type] = getDefaultConfig(type);
        return false;
    }
}

bool ConfigManager::reloadConfigs() {
    bool all_success = true;
    
    for (const auto& [type, path] : config_file_paths_) {
        if (!reloadConfig(type)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool ConfigManager::reloadConfig(ConfigFileType type) {
    auto it = config_file_paths_.find(type);
    if (it == config_file_paths_.end()) {
        return false;
    }
    
    bool success = loadConfig(type, it->second);
    if (success) {
        // 通知所有相关的配置变更回调
        notifyConfigChange(type, "");
    }
    
    return success;
}



nlohmann::json ConfigManager::getComponentConfig(ConfigFileType config_type, const std::string& component_name) const {
    auto config = getConfig(config_type);
    
    // 新的配置结构：配置文件类型 -> 组件类别 -> 组件名
    // 使用现有的configTypeToString函数获取组件类别
    std::string category = configTypeToString(config_type);
    
    if (category == "unknown") {
        return nlohmann::json::object();
    }
    
    if (config.contains(category) && config[category].contains(component_name)) {
        return config[category][component_name];
    }
    
    return nlohmann::json::object();
}

nlohmann::json ConfigManager::getComponentConfig(const std::string& config_type_str, const std::string& component_name) const {
    try {
        ConfigFileType type = stringToConfigType(config_type_str);
        return getComponentConfig(type, component_name);
    } catch (const std::exception&) {
        return nlohmann::json::object();
    }
}

nlohmann::json ConfigManager::getGlobalConfig() const {
    auto core_config = getConfig(ConfigFileType::CORE);
    
    if (core_config.contains("global")) {
        return core_config["global"];
    }
    
    return nlohmann::json::object();
}

nlohmann::json ConfigManager::getConfig(ConfigFileType type) const {
    auto it = configs_.find(type);
    if (it != configs_.end()) {
        return it->second;
    }
    
    return getDefaultConfig(type);
}

void ConfigManager::setConfigValue(ConfigFileType type, const std::string& json_path, const nlohmann::json& value) {
    auto path_components = parseJsonPath(json_path);
    setValueByPath(configs_[type], path_components, value);
    
    // 通知配置变更
    if (!path_components.empty()) {
        notifyConfigChange(type, path_components[0]);
    }
}

bool ConfigManager::saveConfigs() const {
    bool all_success = true;
    
    for (const auto& [type, config] : configs_) {
        if (!saveConfig(type)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool ConfigManager::saveConfig(ConfigFileType type, const std::string& config_file_path) const {
    try {
        std::string filepath = config_file_path;
        if (filepath.empty()) {
            auto it = config_file_paths_.find(type);
            if (it == config_file_paths_.end()) {
                return false;
            }
            filepath = it->second;
        }
        
        auto it = configs_.find(type);
        if (it == configs_.end()) {
            return false;
        }
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << it->second.dump(4);
        file.close();
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ConfigManager::registerConfigChangeCallback(ConfigFileType type, const std::string& section, ConfigChangeCallback callback) {
    callbacks_[type][section] = callback;
}

bool ConfigManager::validateConfigs() const {
    for (const auto& [type, config] : configs_) {
        if (!validateConfig(type)) {
            return false;
        }
    }
    return true;
}

bool ConfigManager::validateConfig(ConfigFileType type) const {
    auto it = configs_.find(type);
    if (it == configs_.end()) {
        return false;
    }
    
    const auto& config = it->second;
    
    // 基本的配置验证
    switch (type) {
        case ConfigFileType::CORE:
            // 验证日志配置
            if (config.contains("logger")) {
                auto logger_config = config["logger"];
                if (logger_config.contains("max_file_size") && logger_config["max_file_size"].get<int>() <= 0) {
                    return false;
                }
                if (logger_config.contains("max_files") && logger_config["max_files"].get<int>() <= 0) {
                    return false;
                }
            }
            // 验证全局配置
            if (config.contains("global")) {
                auto global_config = config["global"];
                if (global_config.contains("simulation_time_step") && global_config["simulation_time_step"].get<double>() <= 0) {
                    return false;
                }
            }
            break;
            
        case ConfigFileType::LOGIC:
            // 验证逻辑组件配置
            if (config.contains("components")) {
                auto components = config["components"];
                for (const auto& [name, comp_config] : components.items()) {
                    if (comp_config.contains("update_frequency") && comp_config["update_frequency"].get<double>() <= 0) {
                        return false;
                    }
                }
            }
            break;
            
        default:
            // 其他类型的基本验证
            break;
    }
    
    return true;
}

nlohmann::json ConfigManager::getDefaultConfig(ConfigFileType type) {
    switch (type) {
        case ConfigFileType::CORE:
            return nlohmann::json::parse(R"({
                "logger": {
                    "console_enabled": true,
                    "file_enabled": true,
                    "file_path": "logs/gnc.log",
                    "max_file_size": 10485760,
                    "max_files": 5,
                    "async_enabled": true,
                    "level": "info"
                },
                "global": {
                    "simulation_time_step": 0.01,
                    "max_simulation_time": 1000.0,
                    "real_time_factor": 1.0
                }
            })");
            
        case ConfigFileType::DYNAMICS:
            return nlohmann::json::parse(R"({
                "dynamics": {
                    "rigid_body_6dof": {
                        "enabled": true,
                        "mass": 1000.0,
                        "inertia_matrix": [[100, 0, 0], [0, 100, 0], [0, 0, 100]],
                        "initial_position": [0, 0, 0],
                        "initial_velocity": [0, 0, 0],
                        "initial_attitude": [0, 0, 0],
                        "initial_angular_velocity": [0, 0, 0]
                    }
                }
            })");
            
        case ConfigFileType::ENVIRONMENT:
            return nlohmann::json::parse(R"({
                "environment": {
                    "atmosphere": {
                        "enabled": true,
                        "sea_level_density": 1.225,
                        "scale_height": 8400.0,
                        "wind_velocity": [0, 0, 0]
                    }
                }
            })");
            
        case ConfigFileType::EFFECTORS:
            return nlohmann::json::parse(R"({
                "effectors": {
                    "aerodynamics": {
                        "enabled": true,
                        "reference_area": 10.0,
                        "drag_coefficient": 0.5,
                        "lift_coefficient": 0.3
                    }
                }
            })");
            
        case ConfigFileType::LOGIC:
            return nlohmann::json::parse(R"({
                "logic": {
                    "navigation": {
                        "enabled": true,
                        "update_frequency": 100.0,
                        "filter_type": "perfect"
                    },
                    "guidance": {
                        "enabled": true,
                        "update_frequency": 50.0,
                        "waypoint_tolerance": 1.0,
                        "max_speed": 10.0
                    },
                    "control": {
                        "enabled": true,
                        "update_frequency": 200.0,
                        "pid_gains": {
                            "kp": 1.0,
                            "ki": 0.1,
                            "kd": 0.01
                        }
                    }
                }
            })");
            
        case ConfigFileType::SENSORS:
            return nlohmann::json::parse(R"({
                "sensors": {
                    "imu": {
                        "enabled": true,
                        "update_frequency": 100.0,
                        "gyro_noise_std": 0.01,
                        "accel_noise_std": 0.05
                    },
                    "gps": {
                        "enabled": true,
                        "update_frequency": 10.0,
                        "position_noise_std": 0.1,
                        "velocity_noise_std": 0.05
                    }
                }
            })");
            
        case ConfigFileType::UTILITY:
            return nlohmann::json::parse(R"({
                "utility": {
                    "logger": {
                        "console_enabled": true,
                        "file_enabled": true,
                        "file_path": "logs/gnc.log",
                        "level": "info"
                    },
                    "bias_adapter": {
                        "enabled": true,
                        "bias_factor": 1.2,
                        "noise_std": 0.01
                    }
                }
            })");
            
        default:
            return nlohmann::json::object();
    }
}

std::string ConfigManager::configTypeToString(ConfigFileType type) {
    switch (type) {
        case ConfigFileType::CORE: return "core";
        case ConfigFileType::DYNAMICS: return "dynamics";
        case ConfigFileType::ENVIRONMENT: return "environment";
        case ConfigFileType::EFFECTORS: return "effectors";
        case ConfigFileType::LOGIC: return "logic";
        case ConfigFileType::SENSORS: return "sensors";
        case ConfigFileType::UTILITY: return "utility";
        default: return "unknown";
    }
}

ConfigFileType ConfigManager::stringToConfigType(const std::string& type_str) {
    if (type_str == "core") return ConfigFileType::CORE;
    if (type_str == "dynamics") return ConfigFileType::DYNAMICS;
    if (type_str == "environment") return ConfigFileType::ENVIRONMENT;
    if (type_str == "effectors") return ConfigFileType::EFFECTORS;
    if (type_str == "logic") return ConfigFileType::LOGIC;
    if (type_str == "sensors") return ConfigFileType::SENSORS;
    if (type_str == "utility") return ConfigFileType::UTILITY;
    
    throw std::invalid_argument("Unknown config type: " + type_str);
}

std::vector<std::string> ConfigManager::parseJsonPath(const std::string& json_path) const {
    std::vector<std::string> path_components;
    std::stringstream ss(json_path);
    std::string component;
    
    while (std::getline(ss, component, '.')) {
        if (!component.empty()) {
            path_components.push_back(component);
        }
    }
    
    return path_components;
}

nlohmann::json ConfigManager::getValueByPath(const nlohmann::json& json, const std::vector<std::string>& path) const {
    nlohmann::json current = json;
    
    for (const auto& component : path) {
        if (current.contains(component)) {
            current = current[component];
        } else {
            return nlohmann::json();
        }
    }
    
    return current;
}

void ConfigManager::setValueByPath(nlohmann::json& json, const std::vector<std::string>& path, const nlohmann::json& value) {
    nlohmann::json* current = &json;
    
    for (size_t i = 0; i < path.size() - 1; ++i) {
        if (!current->contains(path[i])) {
            (*current)[path[i]] = nlohmann::json::object();
        }
        current = &((*current)[path[i]]);
    }
    
    if (!path.empty()) {
        (*current)[path.back()] = value;
    }
}

nlohmann::json ConfigManager::mergeConfigs(const nlohmann::json& base, const nlohmann::json& overlay) const {
    nlohmann::json result = base;
    
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (it.value().is_object() && result.contains(it.key()) && result[it.key()].is_object()) {
            result[it.key()] = mergeConfigs(result[it.key()], it.value());
        } else {
            result[it.key()] = it.value();
        }
    }
    
    return result;
}

void ConfigManager::notifyConfigChange(ConfigFileType type, const std::string& section) {
    auto type_it = callbacks_.find(type);
    if (type_it != callbacks_.end()) {
        auto section_it = type_it->second.find(section);
        if (section_it != type_it->second.end()) {
            auto config = getConfig(type);
            if (section.empty() || config.contains(section)) {
                nlohmann::json section_config = section.empty() ? config : config[section];
                section_it->second(type, section, section_config);
            }
        }
    }
}

} // namespace utility
} // namespace components
} // namespace gnc