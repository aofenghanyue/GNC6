/**
 * @file config_manager.cpp
 * @brief GNC 框架配置管理器实现
 */

#include "gnc/components/utility/config_manager.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace gnc {
namespace components {
namespace utility {

// 自动检测配置文件格式
ConfigFileFormat detectConfigFormat(const std::string& file_path) {
    // 首先根据文件扩展名判断
    std::string extension = std::filesystem::path(file_path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".yaml" || extension == ".yml") {
        return ConfigFileFormat::YAML;
    } else if (extension == ".json") {
        return ConfigFileFormat::JSON;
    }
    
    // 如果扩展名不明确，尝试解析文件内容
    if (std::filesystem::exists(file_path)) {
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::string first_line;
            std::getline(file, first_line);
            file.close();
            
            // 去除空白字符
            first_line.erase(0, first_line.find_first_not_of(" \t\n\r"));
            
            // JSON 通常以 { 开头
            if (!first_line.empty() && first_line[0] == '{') {
                return ConfigFileFormat::JSON;
            }
            
            // YAML 可能以 --- 开头或者直接是键值对
            if (first_line.find("---") == 0 || first_line.find(":") != std::string::npos) {
                return ConfigFileFormat::YAML;
            }
        }
    }
    
    // 默认返回 JSON 格式
    return ConfigFileFormat::JSON;
}

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
        std::string base_name = configTypeToString(type);
        
        // 优先查找 YAML 文件，然后是 JSON 文件
        std::vector<std::string> extensions = {".yaml", ".yml", ".json"};
        bool found = false;
        
        for (const auto& ext : extensions) {
            std::string filepath = config_dir_path + "/" + base_name + ext;
            if (std::filesystem::exists(filepath)) {
                if (loadConfig(type, filepath)) {
                    found = true;
                    break;
                } else {
                    all_success = false;
                }
            }
        }
        
        // 如果没有找到配置文件，创建默认的 JSON 文件
        if (!found) {
            std::string filepath = config_dir_path + "/" + base_name + ".json";
            if (!loadConfig(type, filepath)) {
                all_success = false;
            }
        }
    }
    
    return all_success;
}

bool ConfigManager::loadConfigs(const std::string& config_dir_path, ConfigFileFormat format) {
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
        std::string filename = configTypeToString(type) + getConfigFileExtension(format);
        std::string filepath = config_dir_path + "/" + filename;
        
        if (!loadConfig(type, filepath, format)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool ConfigManager::loadConfig(ConfigFileType type, const std::string& config_file_path) {
    // 自动检测文件格式
    ConfigFileFormat format = detectConfigFormat(config_file_path);
    return loadConfig(type, config_file_path, format);
}

bool ConfigManager::loadConfig(ConfigFileType type, const std::string& config_file_path, ConfigFileFormat format) {
    try {
        config_file_paths_[type] = config_file_path;
        config_file_formats_[type] = format;
        
        if (!std::filesystem::exists(config_file_path)) {
            // 如果文件不存在，创建默认配置文件
            auto default_config = getDefaultConfig(type);
            
            bool save_success = false;
            if (format == ConfigFileFormat::YAML) {
                save_success = saveYamlFile(default_config, config_file_path);
            } else {
                save_success = saveJsonFile(default_config, config_file_path);
            }
            
            if (save_success) {
                configs_[type] = default_config;
                return true;
            } else {
                std::cerr << "Failed to create default config file: " << config_file_path << std::endl;
                configs_[type] = default_config;
                return false;
            }
        }
        
        nlohmann::json config;
        if (format == ConfigFileFormat::YAML) {
            config = loadYamlFile(config_file_path);
        } else {
            config = loadJsonFile(config_file_path);
        }
        
        if (config.is_null()) {
            std::cerr << "Failed to load config file: " << config_file_path << std::endl;
            configs_[type] = getDefaultConfig(type);
            return false;
        }
        
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

bool ConfigManager::saveConfigs(ConfigFileFormat format) const {
    bool all_success = true;
    
    for (const auto& [type, config] : configs_) {
        if (!saveConfig(type, format)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool ConfigManager::saveConfig(ConfigFileType type, const std::string& config_file_path) const {
    // 使用原格式保存
    auto format_it = config_file_formats_.find(type);
    ConfigFileFormat format = (format_it != config_file_formats_.end()) ? format_it->second : ConfigFileFormat::JSON;
    return saveConfig(type, format, config_file_path);
}

bool ConfigManager::saveConfig(ConfigFileType type, ConfigFileFormat format, const std::string& config_file_path) const {
    try {
        std::string filepath = config_file_path;
        if (filepath.empty()) {
            auto it = config_file_paths_.find(type);
            if (it == config_file_paths_.end()) {
                return false;
            }
            filepath = it->second;
            
            // 如果指定了不同的格式，需要更改文件扩展名
            auto current_format_it = config_file_formats_.find(type);
            if (current_format_it != config_file_formats_.end() && current_format_it->second != format) {
                std::filesystem::path path(filepath);
                std::string new_extension = getConfigFileExtension(format);
                filepath = path.parent_path().string() + "/" + path.stem().string() + new_extension;
            }
        }
        
        auto it = configs_.find(type);
        if (it == configs_.end()) {
            return false;
        }
        
        if (format == ConfigFileFormat::YAML) {
            return saveYamlFile(it->second, filepath);
        } else {
            return saveJsonFile(it->second, filepath);
        }
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

// 格式转换方法实现
bool ConfigManager::convertConfigFormat(const std::string& source_path, const std::string& target_path, ConfigFileFormat target_format) const {
    try {
        // 检测源文件格式
        ConfigFileFormat source_format = detectConfigFormat(source_path);
        
        // 加载源文件
        nlohmann::json config;
        if (source_format == ConfigFileFormat::YAML) {
            config = loadYamlFile(source_path);
        } else {
            config = loadJsonFile(source_path);
        }
        
        if (config.is_null()) {
            return false;
        }
        
        // 保存为目标格式
        if (target_format == ConfigFileFormat::YAML) {
            return saveYamlFile(config, target_path);
        } else {
            return saveJsonFile(config, target_path);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error converting config format: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::convertAllConfigs(const std::string& config_dir_path, ConfigFileFormat target_format) const {
    bool all_success = true;
    
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
        std::string base_name = configTypeToString(type);
        
        // 查找现有的配置文件
        std::vector<std::string> extensions = {".yaml", ".yml", ".json"};
        std::string source_path;
        
        for (const auto& ext : extensions) {
            std::string filepath = config_dir_path + "/" + base_name + ext;
            if (std::filesystem::exists(filepath)) {
                source_path = filepath;
                break;
            }
        }
        
        if (!source_path.empty()) {
            std::string target_path = config_dir_path + "/" + base_name + getConfigFileExtension(target_format);
            if (!convertConfigFormat(source_path, target_path, target_format)) {
                all_success = false;
            }
        }
    }
    
    return all_success;
}

// 文件操作方法实现
nlohmann::json ConfigManager::loadJsonFile(const std::string& file_path) const {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return nlohmann::json();
        }
        
        nlohmann::json config;
        file >> config;
        file.close();
        
        return config;
    } catch (const std::exception&) {
        return nlohmann::json();
    }
}

nlohmann::json ConfigManager::loadYamlFile(const std::string& file_path) const {
    try {
        YAML::Node yaml_node = YAML::LoadFile(file_path);
        return yamlToJson(yaml_node);
    } catch (const std::exception&) {
        return nlohmann::json();
    }
}

bool ConfigManager::saveJsonFile(const nlohmann::json& config, const std::string& file_path) const {
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << config.dump(4);
        file.close();
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::saveYamlFile(const nlohmann::json& config, const std::string& file_path) const {
    try {
        YAML::Node yaml_node = jsonToYaml(config);
        
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << yaml_node;
        file.close();
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// YAML 和 JSON 转换方法实现
nlohmann::json ConfigManager::yamlToJson(const YAML::Node& yaml_node) const {
    switch (yaml_node.Type()) {
        case YAML::NodeType::Null:
            return nlohmann::json();
            
        case YAML::NodeType::Scalar: {
            std::string value = yaml_node.as<std::string>();
            
            // 尝试解析为不同类型
            if (value == "true" || value == "True" || value == "TRUE") {
                return true;
            } else if (value == "false" || value == "False" || value == "FALSE") {
                return false;
            } else if (value == "null" || value == "Null" || value == "NULL" || value == "~") {
                return nlohmann::json();
            } else {
                // 尝试解析为数字
                try {
                    if (value.find('.') != std::string::npos) {
                        return std::stod(value);
                    } else {
                        return std::stoll(value);
                    }
                } catch (...) {
                    return value;  // 作为字符串返回
                }
            }
        }
        
        case YAML::NodeType::Sequence: {
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& item : yaml_node) {
                json_array.push_back(yamlToJson(item));
            }
            return json_array;
        }
        
        case YAML::NodeType::Map: {
            nlohmann::json json_object = nlohmann::json::object();
            for (const auto& pair : yaml_node) {
                std::string key = pair.first.as<std::string>();
                json_object[key] = yamlToJson(pair.second);
            }
            return json_object;
        }
        
        default:
            return nlohmann::json();
    }
}

YAML::Node ConfigManager::jsonToYaml(const nlohmann::json& json_obj) const {
    YAML::Node yaml_node;
    
    if (json_obj.is_null()) {
        yaml_node = YAML::Node(YAML::NodeType::Null);
    } else if (json_obj.is_boolean()) {
        yaml_node = json_obj.get<bool>();
    } else if (json_obj.is_number_integer()) {
        yaml_node = json_obj.get<int64_t>();
    } else if (json_obj.is_number_float()) {
        yaml_node = json_obj.get<double>();
    } else if (json_obj.is_string()) {
        yaml_node = json_obj.get<std::string>();
    } else if (json_obj.is_array()) {
        yaml_node = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& item : json_obj) {
            yaml_node.push_back(jsonToYaml(item));
        }
    } else if (json_obj.is_object()) {
        yaml_node = YAML::Node(YAML::NodeType::Map);
        for (const auto& [key, value] : json_obj.items()) {
            yaml_node[key] = jsonToYaml(value);
        }
    }
    
    return yaml_node;
}

std::string ConfigManager::getConfigFileExtension(ConfigFileFormat format) const {
    switch (format) {
        case ConfigFileFormat::YAML:
            return ".yaml";
        case ConfigFileFormat::JSON:
            return ".json";
        default:
            return ".json";
    }
}

} // namespace utility
} // namespace components
} // namespace gnc