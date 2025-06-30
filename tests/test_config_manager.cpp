/**
 * @file test_config_manager.cpp
 * @brief ConfigManager 单元测试
 */

#include <gtest/gtest.h>
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include <iostream>
#include <filesystem>

using namespace gnc::components::utility;

// 测试配置文件加载
TEST(ConfigManagerTest, ConfigLoading) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 测试加载配置文件
    bool result = config_manager.loadConfigs("config/");
    EXPECT_TRUE(result) << "Failed to load configurations";
}

// 测试日志配置获取
TEST(ConfigManagerTest, LoggerConfig) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 获取日志配置
    auto logger_config = config_manager.getComponentConfig(ConfigFileType::UTILITY, "logger");
    
    // 验证日志配置
    EXPECT_TRUE(logger_config.value("console_enabled", false)) << "Console logging should be enabled";
    EXPECT_TRUE(logger_config.value("file_enabled", false)) << "File logging should be enabled";
    EXPECT_FALSE( logger_config.value("file_path", std::string("")).empty()) << "File path should not be empty";
}

// 测试组件配置获取
TEST(ConfigManagerTest, ComponentConfig) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 测试获取组件配置
    auto nav_config = config_manager.getComponentConfig(
        ConfigFileType::LOGIC, "navigation");
    
    EXPECT_FALSE(nav_config.empty()) << "Navigation config should not be empty";
    EXPECT_TRUE(nav_config.contains("enabled")) << "Navigation config should contain 'enabled'";
    
    // 测试获取动力学组件配置
    auto dynamics_config = config_manager.getComponentConfig(
        ConfigFileType::DYNAMICS, "rigid_body_6dof");
    
    EXPECT_FALSE(dynamics_config.empty()) << "Dynamics config should not be empty";
    EXPECT_TRUE(dynamics_config.contains("mass")) << "Dynamics config should contain 'mass'";
    EXPECT_EQ(dynamics_config["mass"], 1000.0) << "Dynamics config mass should be 1000.0";
}

// 测试全局配置获取
TEST(ConfigManagerTest, GlobalConfig) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 获取全局配置
    auto global_config = config_manager.getGlobalConfig();
    
    EXPECT_FALSE(global_config.empty()) << "Global config should not be empty";
    EXPECT_TRUE(global_config.contains("simulation_time_step")) << 
           "Global config should contain 'simulation_time_step'";
    
    // 测试获取具体配置值
    double time_step = config_manager.getConfigValue<double>(
        ConfigFileType::CORE, "global.simulation_time_step", 0.01);
    
    EXPECT_GT(time_step, 0.0) << "Time step should be positive";
}

// 测试配置值设置和获取
TEST(ConfigManagerTest, ConfigValueSetGet) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 设置配置值
    config_manager.setConfigValue(ConfigFileType::CORE, 
        "global.simulation_time_step", 0.005);
    
    // 获取配置值
    double new_time_step = config_manager.getConfigValue<double>(
        ConfigFileType::CORE, "global.simulation_time_step", 0.01);
    
    EXPECT_NEAR(new_time_step, 0.005, 1e-9) << 
           "Time step should be updated to 0.005";
}

// 测试配置验证
TEST(ConfigManagerTest, ConfigValidation) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 验证所有配置
    bool all_valid = config_manager.validateConfigs();
    EXPECT_TRUE(all_valid) << "All configurations should be valid";
    
    // 验证特定配置
    bool core_valid = config_manager.validateConfig(ConfigFileType::CORE);
    EXPECT_TRUE(core_valid) << "Core configuration should be valid";
}

// 测试默认配置
TEST(ConfigManagerTest, DefaultConfig) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 测试获取不存在的配置值，应该返回默认值
    double default_value = config_manager.getConfigValue<double>(
        ConfigFileType::CORE, "non_existent_key", 42.0);
    
    EXPECT_NEAR(default_value, 42.0, 1e-9) << 
           "Should return default value for non-existent key";
    
    // 测试字符串默认值
    std::string default_string = config_manager.getConfigValue<std::string>(
        ConfigFileType::CORE, "non_existent_string", "default_string");
    
    EXPECT_EQ(default_string, "default_string") << 
           "Should return default string for non-existent key";
}

// 测试日志系统集成
TEST(ConfigManagerTest, LoggerIntegration) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 获取日志配置
    auto logger_config = config_manager.getComponentConfig(
        ConfigFileType::UTILITY, "logger");
    
    EXPECT_FALSE(logger_config.empty()) << "Logger config should not be empty";
    
    // 测试日志输出
    auto main_logger = SimpleLogger::getInstance().getMainLogger();
    EXPECT_NE(main_logger, nullptr) << "Main logger should not be null";
    
    if (main_logger) {
        main_logger->info("Test log message from config system");
    }
    
    // 测试组件日志器
    auto comp_logger = SimpleLogger::getInstance().getComponentLogger("test_component");
    EXPECT_NE(comp_logger, nullptr) << "Component logger should not be null";
    
    if (comp_logger) {
        comp_logger->debug("Test component log message");
    }
}