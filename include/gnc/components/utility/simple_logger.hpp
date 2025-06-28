/**
 * @file simple_logger.hpp
 * @brief GNC 框架简单日志系统
 * 
 * @details 日志系统设计
 * 
 * 1. 核心功能
 *    - 统一的日志接口：提供标准的日志记录方法
 *    - 多级别日志：支持 TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL
 *    - 多输出目标：支持控制台、文件等输出
 *    - 高性能：基于 spdlog 实现异步日志
 * 
 * 2. 设计模式
 *    - 单例模式：全局唯一的日志管理器
 *    - 工厂模式：创建不同类型的日志器
 * 
 * 3. 使用方法
 *    @code
 *    // 初始化日志系统
 *    SimpleLogger::initialize("gnc_simulation", LogLevel::INFO);
 *    
 *    // 记录日志
 *    LOG_INFO("Simulation started");
 *    LOG_DEBUG("Vehicle {} position: {}", vehicle_id, position);
 *    LOG_ERROR("Component {} failed: {}", component_name, error_msg);
 *    
 *    // 组件内使用
 *    class MyComponent : public ComponentBase {
 *        void updateImpl() override {
 *            LOG_COMPONENT_INFO("Component updated successfully");
 *        }
 *    };
 *    @endcode
 */
#pragma once
// 告诉spdlog使用编译好的库版本，而不是头文件中的内联实现，从而避免符号重复定义
#ifndef SPDLOG_COMPILED_LIB
#define SPDLOG_COMPILED_LIB
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "../../common/types.hpp"

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE = 0,    ///< 最详细的调试信息
    DEBUG = 1,    ///< 调试信息
    INFO = 2,     ///< 一般信息
    WARN = 3,     ///< 警告信息
    ERR = 4,      ///< 错误信息
    CRITICAL = 5, ///< 严重错误
    OFF = 6       ///< 关闭日志
};

/**
 * @brief 日志输出目标配置
 */
struct LogSinkConfig {
    bool console_enabled = true;           ///< 是否输出到控制台
    bool file_enabled = true;              ///< 是否输出到文件
    std::string file_path = "logs/gnc.log"; ///< 日志文件路径
    size_t max_file_size = 10 * 1024 * 1024; ///< 最大文件大小 (10MB)
    size_t max_files = 5;                  ///< 最大文件数量
    bool async_enabled = true;             ///< 是否启用异步日志
};

/**
 * @brief 简单日志管理器类
 * 
 * 采用单例模式，管理整个应用程序的日志系统
 */
class SimpleLogger {
public:
    /**
     * @brief 获取日志管理器实例
     * @return SimpleLogger& 日志管理器引用
     */
    static SimpleLogger& getInstance();

    /**
     * @brief 初始化日志系统
     * @param logger_name 主日志器名称
     * @param level 日志级别
     * @param config 日志配置
     */
    void initialize(const std::string& logger_name, 
                   LogLevel level = LogLevel::INFO,
                   const LogSinkConfig& config = LogSinkConfig{});



    /**
     * @brief 从多文件配置管理器初始化日志系统
     * 日志器名称从配置文件中读取
     */
    void initializeFromConfig();

    /**
     * @brief 获取主日志器
     * @return std::shared_ptr<spdlog::logger> 日志器指针
     */
    std::shared_ptr<spdlog::logger> getMainLogger();

    /**
     * @brief 获取或创建组件日志器
     * @param component_name 组件名称
     * @return std::shared_ptr<spdlog::logger> 组件日志器指针
     */
    std::shared_ptr<spdlog::logger> getComponentLogger(const std::string& component_name);

    /**
     * @brief 设置日志级别
     * @param level 新的日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 刷新所有日志器
     */
    void flush();

    /**
     * @brief 关闭日志系统
     */
    void shutdown();

private:
    SimpleLogger() = default;
    ~SimpleLogger();
    SimpleLogger(const SimpleLogger&) = delete;
    SimpleLogger& operator=(const SimpleLogger&) = delete;

    /**
     * @brief 转换日志级别
     * @param level GNC日志级别
     * @return spdlog::level::level_enum spdlog日志级别
     */
    spdlog::level::level_enum toSpdlogLevel(LogLevel level);

    /**
     * @brief 创建日志输出目标
     * @param config 日志配置
     * @return std::vector<spdlog::sink_ptr> 输出目标列表
     */
    std::vector<spdlog::sink_ptr> createSinks(const LogSinkConfig& config);

private:
    std::shared_ptr<spdlog::logger> main_logger_;           ///< 主日志器
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> component_loggers_; ///< 组件日志器映射
    std::vector<spdlog::sink_ptr> sinks_;                   ///< 日志输出目标
    LogLevel current_level_ = LogLevel::INFO;               ///< 当前日志级别
    bool initialized_ = false;                              ///< 是否已初始化
};

} // namespace utility
} // namespace components
} // namespace gnc

// ============================================================================
// 便捷宏定义
// ============================================================================

/**
 * @brief 主日志器宏定义
 */
#define LOG_TRACE(...)    if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) if(gnc::components::utility::SimpleLogger::getInstance().getMainLogger()) gnc::components::utility::SimpleLogger::getInstance().getMainLogger()->critical(__VA_ARGS__)

/**
 * @brief 组件日志器宏定义
 * 在组件类中使用，自动使用组件名称作为日志器名称
 */
#define LOG_COMPONENT_TRACE(...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->trace(__VA_ARGS__)
#define LOG_COMPONENT_DEBUG(...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->debug(__VA_ARGS__)
#define LOG_COMPONENT_INFO(...)     if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->info(__VA_ARGS__)
#define LOG_COMPONENT_WARN(...)     if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->warn(__VA_ARGS__)
#define LOG_COMPONENT_ERROR(...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->error(__VA_ARGS__)
#define LOG_COMPONENT_CRITICAL(...) if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(this->getName().c_str())) logger->critical(__VA_ARGS__)

/**
 * @brief 带组件名称的日志宏定义
 * 用于在组件外部记录特定组件的日志
 */
#define LOG_COMPONENT_NAMED_TRACE(name, ...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->trace(__VA_ARGS__)
#define LOG_COMPONENT_NAMED_DEBUG(name, ...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->debug(__VA_ARGS__)
#define LOG_COMPONENT_NAMED_INFO(name, ...)     if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->info(__VA_ARGS__)
#define LOG_COMPONENT_NAMED_WARN(name, ...)     if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->warn(__VA_ARGS__)
#define LOG_COMPONENT_NAMED_ERROR(name, ...)    if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->error(__VA_ARGS__)
#define LOG_COMPONENT_NAMED_CRITICAL(name, ...) if(auto logger = gnc::components::utility::SimpleLogger::getInstance().getComponentLogger(name)) logger->critical(__VA_ARGS__)
