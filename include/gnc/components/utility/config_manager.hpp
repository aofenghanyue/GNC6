/**
 * @file config_manager.hpp
 * @brief GNC 框架配置管理器
 * 
 * @details 配置管理系统设计
 * 
 * 1. 核心功能
 *    - JSON配置文件读取：支持按组件类型分离的配置文件
 *    - 统一配置接口：提供统一的配置访问接口
 *    - 参数验证：确保配置参数的有效性
 *    - 热重载：支持运行时重新加载配置
 * 
 * 2. 配置文件分类
 *    - core.json: 核心系统配置（日志、全局参数等）
 *    - dynamics.json: 动力学组件配置
 *    - environment.json: 环境组件配置
 *    - effectors.json: 效应器组件配置
 *    - logic.json: 逻辑组件配置（导航、制导、控制）
 *    - sensors.json: 传感器组件配置
 *    - utility.json: 工具组件配置
 * 
 * 3. 使用方法
 *    @code
 *    // 初始化配置管理器
 *    ConfigManager::getInstance().loadConfigs("config/");
 *    
 *    // 获取日志配置
     *    auto logConfig = ConfigManager::getInstance().getComponentConfig(ConfigFileType::UTILITY, "logger");
 *    
 *    // 获取组件配置
 *    auto navConfig = ConfigManager::getInstance().getComponentConfig("logic", "navigation");
 *    @endcode
 */
#pragma once

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "simple_logger.hpp"

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief 配置文件类型枚举
 */
enum class ConfigFileType {
    CORE,        // 核心配置（日志、全局参数）
    DYNAMICS,    // 动力学组件
    ENVIRONMENT, // 环境组件
    EFFECTORS,   // 效应器组件
    LOGIC,       // 逻辑组件（导航、制导、控制）
    SENSORS,     // 传感器组件
    UTILITY      // 工具组件
};

/**
 * @brief 配置文件格式枚举
 */
enum class ConfigFileFormat {
    JSON,        // JSON 格式
    YAML         // YAML 格式
};

/**
 * @brief 自动检测配置文件格式
 * @param file_path 文件路径
 * @return ConfigFileFormat 检测到的文件格式
 */
ConfigFileFormat detectConfigFormat(const std::string& file_path);

/**
 * @brief 配置变更回调函数类型
 */
using ConfigChangeCallback = std::function<void(ConfigFileType type, const std::string& section, const nlohmann::json& config)>;

/**
 * @brief 配置管理器类
 * 
 * 采用单例模式，管理按组件类型分离的配置文件系统
 */
class ConfigManager {
public:
    /**
     * @brief 获取配置管理器实例
     * @return ConfigManager& 配置管理器引用
     */
    static ConfigManager& getInstance();

    /**
     * @brief 加载所有配置文件（自动检测格式）
     * @param config_dir_path 配置文件目录路径
     * @return bool 加载是否成功
     */
    bool loadConfigs(const std::string& config_dir_path);

    /**
     * @brief 加载所有配置文件（指定格式）
     * @param config_dir_path 配置文件目录路径
     * @param format 配置文件格式
     * @return bool 加载是否成功
     */
    bool loadConfigs(const std::string& config_dir_path, ConfigFileFormat format);

    /**
     * @brief 加载单个配置文件（自动检测格式）
     * @param type 配置文件类型
     * @param config_file_path 配置文件路径
     * @return bool 加载是否成功
     */
    bool loadConfig(ConfigFileType type, const std::string& config_file_path);

    /**
     * @brief 加载单个配置文件（指定格式）
     * @param type 配置文件类型
     * @param config_file_path 配置文件路径
     * @param format 配置文件格式
     * @return bool 加载是否成功
     */
    bool loadConfig(ConfigFileType type, const std::string& config_file_path, ConfigFileFormat format);

    /**
     * @brief 重新加载所有配置文件
     * @return bool 重新加载是否成功
     */
    bool reloadConfigs();

    /**
     * @brief 重新加载指定类型的配置文件
     * @param type 配置文件类型
     * @return bool 重新加载是否成功
     */
    bool reloadConfig(ConfigFileType type);



    /**
     * @brief 获取组件配置
     * @param config_type 配置文件类型
     * @param component_name 组件名称
     * @return nlohmann::json 组件配置JSON对象
     */
    nlohmann::json getComponentConfig(ConfigFileType config_type, const std::string& component_name) const;

    /**
     * @brief 获取组件配置（通过字符串类型）
     * @param config_type_str 配置文件类型字符串
     * @param component_name 组件名称
     * @return nlohmann::json 组件配置JSON对象
     */
    nlohmann::json getComponentConfig(const std::string& config_type_str, const std::string& component_name) const;

    /**
     * @brief 获取全局配置
     * @return nlohmann::json 全局配置JSON对象
     */
    nlohmann::json getGlobalConfig() const;

    /**
     * @brief 获取指定类型的完整配置
     * @param type 配置文件类型
     * @return nlohmann::json 配置JSON对象
     */
    nlohmann::json getConfig(ConfigFileType type) const;

    /**
     * @brief 获取指定路径的配置值
     * @tparam T 返回值类型
     * @param type 配置文件类型
     * @param json_path JSON路径（如 "navigation.update_frequency"）
     * @param default_value 默认值
     * @return T 配置值
     */
    template<typename T>
    T getConfigValue(ConfigFileType type, const std::string& json_path, const T& default_value) const;

    /**
     * @brief 设置配置值
     * @param type 配置文件类型
     * @param json_path JSON路径
     * @param value 配置值
     */
    void setConfigValue(ConfigFileType type, const std::string& json_path, const nlohmann::json& value);

    /**
     * @brief 保存所有配置到文件（使用原格式）
     * @return bool 保存是否成功
     */
    bool saveConfigs() const;

    /**
     * @brief 保存所有配置到文件（指定格式）
     * @param format 保存格式
     * @return bool 保存是否成功
     */
    bool saveConfigs(ConfigFileFormat format) const;

    /**
     * @brief 保存指定类型的配置到文件（使用原格式）
     * @param type 配置文件类型
     * @param config_file_path 配置文件路径（可选）
     * @return bool 保存是否成功
     */
    bool saveConfig(ConfigFileType type, const std::string& config_file_path = "") const;

    /**
     * @brief 保存指定类型的配置到文件（指定格式）
     * @param type 配置文件类型
     * @param format 保存格式
     * @param config_file_path 配置文件路径（可选）
     * @return bool 保存是否成功
     */
    bool saveConfig(ConfigFileType type, ConfigFileFormat format, const std::string& config_file_path = "") const;

    /**
     * @brief 转换配置文件格式
     * @param source_path 源文件路径
     * @param target_path 目标文件路径
     * @param target_format 目标格式
     * @return bool 转换是否成功
     */
    bool convertConfigFormat(const std::string& source_path, const std::string& target_path, ConfigFileFormat target_format) const;

    /**
     * @brief 批量转换配置文件格式
     * @param config_dir_path 配置目录路径
     * @param target_format 目标格式
     * @return bool 转换是否成功
     */
    bool convertAllConfigs(const std::string& config_dir_path, ConfigFileFormat target_format) const;

    /**
     * @brief 注册配置变更回调
     * @param type 配置文件类型
     * @param section 配置节名称
     * @param callback 回调函数
     */
    void registerConfigChangeCallback(ConfigFileType type, const std::string& section, ConfigChangeCallback callback);

    /**
     * @brief 验证所有配置文件
     * @return bool 配置是否有效
     */
    bool validateConfigs() const;

    /**
     * @brief 验证指定类型的配置文件
     * @param type 配置文件类型
     * @return bool 配置是否有效
     */
    bool validateConfig(ConfigFileType type) const;

    /**
     * @brief 获取默认配置
     * @param type 配置文件类型
     * @return nlohmann::json 默认配置JSON对象
     */
    static nlohmann::json getDefaultConfig(ConfigFileType type);

    /**
     * @brief 配置文件类型转字符串
     * @param type 配置文件类型
     * @return std::string 类型字符串
     */
    static std::string configTypeToString(ConfigFileType type);

    /**
     * @brief 字符串转配置文件类型
     * @param type_str 类型字符串
     * @return ConfigFileType 配置文件类型
     */
    static ConfigFileType stringToConfigType(const std::string& type_str);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief 解析JSON路径
     * @param json_path JSON路径字符串
     * @return std::vector<std::string> 路径组件列表
     */
    std::vector<std::string> parseJsonPath(const std::string& json_path) const;

    /**
     * @brief 根据路径获取JSON值
     * @param json JSON对象
     * @param path 路径组件列表
     * @return nlohmann::json 值
     */
    nlohmann::json getValueByPath(const nlohmann::json& json, const std::vector<std::string>& path) const;

    /**
     * @brief 根据路径设置JSON值
     * @param json JSON对象引用
     * @param path 路径组件列表
     * @param value 要设置的值
     */
    void setValueByPath(nlohmann::json& json, const std::vector<std::string>& path, const nlohmann::json& value);

    /**
     * @brief 合并配置
     * @param base 基础配置
     * @param overlay 覆盖配置
     * @return nlohmann::json 合并后的配置
     */
    nlohmann::json mergeConfigs(const nlohmann::json& base, const nlohmann::json& overlay) const;

    /**
     * @brief 通知配置变更
     * @param type 配置文件类型
     * @param section 配置节名称
     */
    void notifyConfigChange(ConfigFileType type, const std::string& section);

    /**
     * @brief 从JSON文件加载配置
     * @param file_path 文件路径
     * @return nlohmann::json 配置对象
     */
    nlohmann::json loadJsonFile(const std::string& file_path) const;

    /**
     * @brief 从YAML文件加载配置
     * @param file_path 文件路径
     * @return nlohmann::json 配置对象
     */
    nlohmann::json loadYamlFile(const std::string& file_path) const;

    /**
     * @brief 保存配置到JSON文件
     * @param config 配置对象
     * @param file_path 文件路径
     * @return bool 保存是否成功
     */
    bool saveJsonFile(const nlohmann::json& config, const std::string& file_path) const;

    /**
     * @brief 保存配置到YAML文件
     * @param config 配置对象
     * @param file_path 文件路径
     * @return bool 保存是否成功
     */
    bool saveYamlFile(const nlohmann::json& config, const std::string& file_path) const;

    /**
     * @brief 将YAML节点转换为JSON对象
     * @param yaml_node YAML节点
     * @return nlohmann::json JSON对象
     */
    nlohmann::json yamlToJson(const YAML::Node& yaml_node) const;

    /**
     * @brief 将JSON对象转换为YAML节点
     * @param json_obj JSON对象
     * @return YAML::Node YAML节点
     */
    YAML::Node jsonToYaml(const nlohmann::json& json_obj) const;

    /**
     * @brief 获取配置文件的扩展名
     * @param format 配置文件格式
     * @return std::string 文件扩展名
     */
    std::string getConfigFileExtension(ConfigFileFormat format) const;

    // 成员变量
    std::unordered_map<ConfigFileType, nlohmann::json> configs_;  ///< 配置存储
    std::unordered_map<ConfigFileType, std::string> config_file_paths_;  ///< 配置文件路径
    std::unordered_map<ConfigFileType, ConfigFileFormat> config_file_formats_;  ///< 配置文件格式
    std::string config_dir_path_;  ///< 配置目录路径
    std::unordered_map<ConfigFileType, std::unordered_map<std::string, ConfigChangeCallback>> callbacks_;  ///< 配置变更回调
};

// 模板方法实现
template<typename T>
T ConfigManager::getConfigValue(ConfigFileType type, const std::string& json_path, const T& default_value) const {
    try {
        auto it = configs_.find(type);
        if (it == configs_.end()) {
            return default_value;
        }

        auto path_components = parseJsonPath(json_path);
        auto value = getValueByPath(it->second, path_components);
        
        if (value.is_null()) {
            return default_value;
        }
        
        return value.get<T>();
    } catch (const std::exception&) {
        return default_value;
    }
}

} // namespace utility
} // namespace components
} // namespace gnc