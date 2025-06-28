/**
 * @file logger_example.cpp
 * @brief GNC 日志系统使用示例
 * 
 * 本文件演示了如何在 GNC 框架中使用日志系统：
 * 1. 基本日志记录
 * 2. 组件内日志记录
 * 3. 不同日志级别的使用
 * 4. 日志配置选项
 */

#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/core/component_base.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace gnc::components::utility;

/**
 * @brief 示例组件类，演示组件内日志使用
 */
class ExampleComponent : public gnc::states::ComponentBase {
public:
    ExampleComponent(gnc::states::VehicleId vehicle_id) 
        : ComponentBase(vehicle_id, "ExampleComponent") {
        LOG_COMPONENT_INFO("ExampleComponent created for vehicle {}", vehicle_id);
    }
    
    void demonstrateLogging() {
        LOG_COMPONENT_TRACE("This is a trace message - very detailed debugging info");
        LOG_COMPONENT_DEBUG("This is a debug message - general debugging info");
        LOG_COMPONENT_INFO("This is an info message - general information");
        LOG_COMPONENT_WARN("This is a warning message - something might be wrong");
        LOG_COMPONENT_ERROR("This is an error message - something went wrong");
        
        // 演示格式化日志
        double value = 3.14159;
        int count = 42;
        std::string status = "active";
        
        LOG_COMPONENT_INFO("Formatted log: value={:.2f}, count={}, status={}", 
                          value, count, status);
    }
    
protected:
    void updateImpl() override {
        LOG_COMPONENT_DEBUG("Component update called");
        // 模拟一些工作
        static int update_count = 0;
        update_count++;
        
        if (update_count % 10 == 0) {
            LOG_COMPONENT_INFO("Completed {} updates", update_count);
        }
        
        if (update_count > 50) {
            LOG_COMPONENT_WARN("Update count is getting high: {}", update_count);
        }
    }
};

/**
 * @brief 演示基本日志功能
 */
void demonstrateBasicLogging() {
    LOG_INFO("=== Basic Logging Demonstration ===");
    
    // 不同级别的日志
    LOG_TRACE("Trace: Very detailed debugging information");
    LOG_DEBUG("Debug: General debugging information");
    LOG_INFO("Info: General information about program execution");
    LOG_WARN("Warning: Something unexpected happened, but not critical");
    LOG_ERROR("Error: Something went wrong, but program can continue");
    LOG_CRITICAL("Critical: Serious error, program might not continue");
    
    // 格式化日志
    std::string user = "Alice";
    int age = 30;
    double score = 95.5;
    
    LOG_INFO("User info: name={}, age={}, score={:.1f}", user, age, score);
    
    // 条件日志
    bool debug_mode = true;
    if (debug_mode) {
        LOG_DEBUG("Debug mode is enabled");
    }
}

/**
 * @brief 演示不同的日志配置
 */
void demonstrateLogConfiguration() {
    LOG_INFO("=== Log Configuration Demonstration ===");
    
    // 创建自定义日志配置
    LogSinkConfig custom_config;
    custom_config.console_enabled = true;
    custom_config.file_enabled = true;
    custom_config.file_path = "logs/custom_example.log";
    custom_config.max_file_size = 5 * 1024 * 1024; // 5MB
    custom_config.max_files = 3;
    custom_config.async_enabled = true;
    
    LOG_INFO("Current configuration:");
    LOG_INFO("  Console output: {}", custom_config.console_enabled ? "enabled" : "disabled");
    LOG_INFO("  File output: {}", custom_config.file_enabled ? "enabled" : "disabled");
    LOG_INFO("  Log file: {}", custom_config.file_path.c_str());
    LOG_INFO("  Max file size: {} bytes", custom_config.max_file_size);
    LOG_INFO("  Max files: {}", custom_config.max_files);
    LOG_INFO("  Async logging: {}", custom_config.async_enabled ? "enabled" : "disabled");
}

/**
 * @brief 演示日志级别控制
 */
void demonstrateLogLevels() {
    LOG_INFO("=== Log Level Control Demonstration ===");
    
    auto& logger = SimpleLogger::getInstance();
    
    // 测试不同日志级别
    std::vector<LogLevel> levels = {
        LogLevel::TRACE,
        LogLevel::DEBUG, 
        LogLevel::INFO,
        LogLevel::WARN,
        LogLevel::ERR
    };
    
    std::vector<std::string> level_names = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR"
    };
    
    for (size_t i = 0; i < levels.size(); ++i) {
        LOG_INFO("Setting log level to: {}", level_names[i]);
        logger.setLogLevel(levels[i]);
        
        // 测试各级别日志输出
        LOG_TRACE("This is a TRACE message");
        LOG_DEBUG("This is a DEBUG message");
        LOG_INFO("This is an INFO message");
        LOG_WARN("This is a WARN message");
        LOG_ERROR("This is an ERROR message");
        
        LOG_INFO("--- End of level {} test ---\n", level_names[i]);
        
        // 短暂暂停以便观察输出
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 恢复到 INFO 级别
    logger.setLogLevel(LogLevel::INFO);
    LOG_INFO("Log level restored to INFO");
}

/**
 * @brief 演示组件日志功能
 */
void demonstrateComponentLogging() {
    LOG_INFO("=== Component Logging Demonstration ===");
    
    // 创建示例组件
    ExampleComponent component(1);
    
    // 演示组件日志
    component.demonstrateLogging();
    
    // 模拟组件更新
    LOG_INFO("Simulating component updates...");
    for (int i = 0; i < 15; ++i) {
        // component.updateImpl();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 使用命名组件日志
    LOG_COMPONENT_NAMED_INFO("ExampleComponent", "Using named component logger");
    LOG_COMPONENT_NAMED_WARN("ExampleComponent", "This is a warning from named logger");
}

int main() {
    try {
        std::cout << "=== GNC Logger System Example ===" << std::endl;
        
        // 初始化日志系统
        LogSinkConfig config;
        config.console_enabled = true;
        config.file_enabled = true;
        config.file_path = "logs/logger_example.log";
        config.async_enabled = false; // 使用同步日志以便观察输出顺序
        
        SimpleLogger::getInstance().initialize(
            "logger_example",
            LogLevel::TRACE, // 设置为最详细级别
            config
        );
        
        LOG_INFO("Logger example started");
        
        // 演示各种日志功能
        demonstrateBasicLogging();
        std::cout << std::endl;
        
        demonstrateLogConfiguration();
        std::cout << std::endl;
        
        demonstrateLogLevels();
        std::cout << std::endl;
        
        demonstrateComponentLogging();
        std::cout << std::endl;
        
        LOG_INFO("All demonstrations completed successfully");
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Exception in logger example: {}", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        SimpleLogger::getInstance().shutdown();
        return 1;
    }
    
    // 关闭日志系统
    LOG_INFO("Logger example finished");
    SimpleLogger::getInstance().shutdown();
    
    std::cout << "\n=== Example completed. Check logs/logger_example.log for file output ===" << std::endl;
    return 0;
}