/**
 * @file yaml_config_example.cpp
 * @brief YAML 配置管理器使用示例
 * 
 * 本示例展示了如何使用扩展后的配置管理器来处理 YAML 格式的配置文件，
 * 包括加载、保存、格式转换等功能。
 */

#include "gnc/components/utility/config_manager.hpp"
#include <iostream>
#include <filesystem>

using namespace gnc::components::utility;

int main() {
    std::cout << "=== GNC YAML 配置管理器示例 ===\n\n";
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 1. 加载 YAML 配置文件
    std::cout << "1. 加载 YAML 配置文件...\n";
    if (config_manager.loadConfigs("config/")) {
        std::cout << "   ✓ 配置文件加载成功\n";
    } else {
        std::cout << "   ✗ 配置文件加载失败\n";
        return -1;
    }
    
    // 2. 读取配置值
    std::cout << "\n2. 读取配置值...\n";
    
    // 读取日志配置
    auto log_level = config_manager.getConfigValue<std::string>(
        ConfigFileType::CORE, "logger.level", "info");
    std::cout << "   日志级别: " << log_level << "\n";
    
    auto max_file_size = config_manager.getConfigValue<int>(
        ConfigFileType::CORE, "logger.max_file_size", 0);
    std::cout << "   最大日志文件大小: " << max_file_size << " bytes\n";
    
    // 读取导航配置
    auto nav_frequency = config_manager.getConfigValue<double>(
        ConfigFileType::LOGIC, "logic.navigation.update_frequency", 0.0);
    std::cout << "   导航更新频率: " << nav_frequency << " Hz\n";
    
    // 读取 PID 控制器参数
    auto kp = config_manager.getConfigValue<double>(
        ConfigFileType::LOGIC, "logic.control.pid_gains.position.kp", 0.0);
    std::cout << "   位置控制 Kp: " << kp << "\n";
    
    // 3. 修改配置值
    std::cout << "\n3. 修改配置值...\n";
    config_manager.setConfigValue(ConfigFileType::CORE, "logger.level", "debug");
    config_manager.setConfigValue(ConfigFileType::LOGIC, "logic.navigation.update_frequency", 150.0);
    std::cout << "   ✓ 配置值已修改\n";
    
    // 4. 保存配置文件（保持原格式）
    std::cout << "\n4. 保存配置文件...\n";
    if (config_manager.saveConfigs()) {
        std::cout << "   ✓ 配置文件保存成功\n";
    } else {
        std::cout << "   ✗ 配置文件保存失败\n";
    }
    
    // 5. 格式转换示例
    std::cout << "\n5. 配置文件格式转换...\n";
    
    // 将所有配置转换为 JSON 格式
    std::cout << "   转换为 JSON 格式...\n";
    if (config_manager.convertAllConfigs("config/", ConfigFileFormat::JSON)) {
        std::cout << "   ✓ 转换为 JSON 格式成功\n";
    } else {
        std::cout << "   ✗ 转换为 JSON 格式失败\n";
    }
    
    // 将所有配置转换为 YAML 格式
    std::cout << "   转换为 YAML 格式...\n";
    if (config_manager.convertAllConfigs("config/", ConfigFileFormat::YAML)) {
        std::cout << "   ✓ 转换为 YAML 格式成功\n";
    } else {
        std::cout << "   ✗ 转换为 YAML 格式失败\n";
    }
    
    // 6. 单个文件格式转换
    std::cout << "\n6. 单个文件格式转换...\n";
    if (config_manager.convertConfigFormat(
        "config/core.yaml", "config/core_backup.json", ConfigFileFormat::JSON)) {
        std::cout << "   ✓ core.yaml -> core_backup.json 转换成功\n";
    } else {
        std::cout << "   ✗ 文件转换失败\n";
    }
    
    // 7. 加载指定格式的配置
    std::cout << "\n7. 加载指定格式的配置...\n";
    
    // 强制加载 YAML 格式
    if (config_manager.loadConfigs("config/", ConfigFileFormat::YAML)) {
        std::cout << "   ✓ YAML 格式配置加载成功\n";
    } else {
        std::cout << "   ✗ YAML 格式配置加载失败\n";
    }
    
    // 8. 保存为指定格式
    std::cout << "\n8. 保存为指定格式...\n";
    
    // 保存为 YAML 格式
    if (config_manager.saveConfigs(ConfigFileFormat::YAML)) {
        std::cout << "   ✓ 保存为 YAML 格式成功\n";
    } else {
        std::cout << "   ✗ 保存为 YAML 格式失败\n";
    }
    
    // 9. 显示配置文件信息
    std::cout << "\n9. 配置文件信息...\n";
    
    std::vector<std::string> config_files = {
        "config/core.yaml",
        "config/logic.yaml",
        "config/core.json",
        "config/logic.json"
    };
    
    for (const auto& file : config_files) {
        if (std::filesystem::exists(file)) {
            auto format = detectConfigFormat(file);
            std::string format_str = (format == ConfigFileFormat::YAML) ? "YAML" : "JSON";
            auto size = std::filesystem::file_size(file);
            std::cout << "   " << file << " (" << format_str << ", " << size << " bytes)\n";
        }
    }
    
    std::cout << "\n=== 示例完成 ===\n";
    
    return 0;
}