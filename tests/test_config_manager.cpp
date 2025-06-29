/**
 * @file test_config_manager.cpp
 * @brief ConfigManager 单元测试
 */

#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace gnc::components::utility;

/**
 * @brief 测试配置文件加载
 */
void testConfigLoading() {
    std::cout << "Testing config loading..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 测试加载配置文件
    bool result = config_manager.loadConfigs("config/");
    assert(result && "Failed to load configurations");
    
    std::cout << "✓ Config loading test passed" << std::endl;
}

/**
 * @brief 测试日志配置获取
 */
void testLoggerConfig() {
    std::cout << "Testing logger config..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 获取日志配置
    auto logger_config = config_manager.getComponentConfig(ConfigFileType::UTILITY, "logger");
    
    // 验证日志配置
    assert(logger_config.value("console_enabled", false) && "Console logging should be enabled");
    assert(logger_config.value("file_enabled", false) && "File logging should be enabled");
    assert(!logger_config.value("file_path", std::string("")).empty() && "File path should not be empty");
    
    std::cout << "✓ Logger config test passed" << std::endl;
}

/**
 * @brief 测试组件配置获取
 */
void testComponentConfig() {
    std::cout << "Testing component config..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 测试获取组件配置
    auto nav_config = config_manager.getComponentConfig(
        ConfigFileType::LOGIC, "navigation");
    
    assert(!nav_config.empty() && "Navigation config should not be empty");
    assert(nav_config.contains("enabled") && "Navigation config should contain 'enabled'");
    
    // 测试获取动力学组件配置
    auto dynamics_config = config_manager.getComponentConfig(
        ConfigFileType::DYNAMICS, "rigid_body_6dof");
    
    assert(!dynamics_config.empty() && "Dynamics config should not be empty");
    assert(dynamics_config.contains("mass") && "Dynamics config should contain 'mass'");
    assert(dynamics_config["mass"]==1000.0 && "Dynamics config mass should be 1000.0");
    
    std::cout << "✓ Component config test passed" << std::endl;
}

/**
 * @brief 测试全局配置获取
 */
void testGlobalConfig() {
    std::cout << "Testing global config..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 获取全局配置
    auto global_config = config_manager.getGlobalConfig();
    
    assert(!global_config.empty() && "Global config should not be empty");
    assert(global_config.contains("simulation_time_step") && 
           "Global config should contain 'simulation_time_step'");
    
    // 测试获取具体配置值
    double time_step = config_manager.getConfigValue<double>(
        ConfigFileType::CORE, "global.simulation_time_step", 0.01);
    
    assert(time_step > 0.0 && "Time step should be positive");
    
    std::cout << "✓ Global config test passed" << std::endl;
}

/**
 * @brief 测试配置值设置和获取
 */
void testConfigValueSetGet() {
    std::cout << "Testing config value set/get..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 设置配置值
    config_manager.setConfigValue(ConfigFileType::CORE, 
        "global.simulation_time_step", 0.005);
    
    // 获取配置值
    double new_time_step = config_manager.getConfigValue<double>(
        ConfigFileType::CORE, "global.simulation_time_step", 0.01);
    
    assert(std::abs(new_time_step - 0.005) < 1e-9 && 
           "Time step should be updated to 0.005");
    
    std::cout << "✓ Config value set/get test passed" << std::endl;
}

/**
 * @brief 测试配置验证
 */
void testConfigValidation() {
    std::cout << "Testing config validation..." << std::endl;
    
    auto& config_manager = ConfigManager::getInstance();
    
    // 验证所有配置
    bool all_valid = config_manager.validateConfigs();
    assert(all_valid && "All configurations should be valid");
    
    // 验证特定配置
    bool core_valid = config_manager.validateConfig(ConfigFileType::CORE);
    assert(core_valid && "Core configuration should be valid");
    
    std::cout << "✓ Config validation test passed" << std::endl;
}

/**
 * @brief 测试默认配置生成
 */
void testDefaultConfig() {
    std::cout << "Testing default config generation..." << std::endl;
    
    // 获取默认配置
    auto default_core = ConfigManager::getDefaultConfig(ConfigFileType::CORE);
    auto default_logic = ConfigManager::getDefaultConfig(ConfigFileType::LOGIC);
    
    assert(!default_core.empty() && "Default core config should not be empty");
    assert(!default_logic.empty() && "Default logic config should not be empty");
    
    // 验证默认配置包含必要字段
    assert(default_core.contains("logger") && "Default core config should contain logger");
    assert(default_core.contains("global") && "Default core config should contain global");
    
    std::cout << "✓ Default config test passed" << std::endl;
}

/**
 * @brief 测试日志系统集成
 */
void testLoggerIntegration() {
    std::cout << "Testing logger integration..." << std::endl;
    
    // 日志系统现在会自动初始化，logger名称从配置文件读取
    
    // 测试日志输出
    auto main_logger = SimpleLogger::getInstance().getMainLogger();
    assert(main_logger != nullptr && "Main logger should not be null");
    
    main_logger->info("Test log message from config system");
    
    // 测试组件日志器
    auto comp_logger = SimpleLogger::getInstance().getComponentLogger("test_component");
    assert(comp_logger != nullptr && "Component logger should not be null");
    
    comp_logger->debug("Test component log message");
    
    std::cout << "✓ Logger integration test passed" << std::endl;
}

/**
 * @brief 运行所有测试
 */
int main() {
    try {
        std::cout << "=== ConfigManager Unit Tests ===" << std::endl;
        
        testConfigLoading();
        testLoggerConfig();
        testComponentConfig();
        testGlobalConfig();
        testConfigValueSetGet();
        testConfigValidation();
        testDefaultConfig();
        testLoggerIntegration();
        
        std::cout << "\n=== All Tests Passed! ===" << std::endl;
        
        // 关闭日志系统
        SimpleLogger::getInstance().shutdown();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}