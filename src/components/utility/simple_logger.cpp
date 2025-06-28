/**
 * @file simple_logger.cpp
 * @brief GNC 框架简单日志系统实现
 */

#include "../../../include/gnc/components/utility/simple_logger.hpp"
#include "../../../include/gnc/components/utility/config_manager.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <iostream>

namespace gnc {
namespace components {
namespace utility {

// ============================================================================
// SimpleLogger 实现
// ============================================================================

SimpleLogger& SimpleLogger::getInstance() {
    static SimpleLogger instance;
    
    // 自动初始化：如果还未初始化，则从配置文件自动初始化
    if (!instance.initialized_) {
        instance.initializeFromConfig();
    }
    
    return instance;
}

SimpleLogger::~SimpleLogger() {
    shutdown();
}

void SimpleLogger::initialize(const std::string& logger_name, 
                             LogLevel level,
                             const LogSinkConfig& config) {
    if (initialized_) {
        if (main_logger_) {
            main_logger_->warn("Logger already initialized, skipping re-initialization");
        }
        return;
    }

    try {
        // 设置当前日志级别
        current_level_ = level;
        
        // 创建日志输出目标
        sinks_ = createSinks(config);
        
        if (sinks_.empty()) {
            std::cerr << "Failed to create any log sinks" << std::endl;
            return;
        }

        // 如果启用异步日志，初始化异步线程池
        if (config.async_enabled) {
            // 初始化异步日志线程池：8192 队列大小，1个后台线程
            spdlog::init_thread_pool(8192, 1);
            
            // 创建异步主日志器
            main_logger_ = std::make_shared<spdlog::async_logger>(
                logger_name, 
                sinks_.begin(), 
                sinks_.end(), 
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            // 创建同步主日志器
            main_logger_ = std::make_shared<spdlog::logger>(logger_name, sinks_.begin(), sinks_.end());
        }

        // 设置日志级别
        main_logger_->set_level(toSpdlogLevel(level));
        
        // 设置日志格式：[时间] [级别] [日志器名] 消息
        main_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
        
        // 注册主日志器到 spdlog
        spdlog::register_logger(main_logger_);
        
        // 设置为默认日志器
        spdlog::set_default_logger(main_logger_);
        
        initialized_ = true;
        
        main_logger_->info("GNC Logger initialized successfully");
        main_logger_->info("Log level: {}", static_cast<int>(level));
        main_logger_->info("Console output: {}", config.console_enabled ? "enabled" : "disabled");
        main_logger_->info("File output: {}", config.file_enabled ? "enabled" : "disabled");
        if (config.file_enabled) {
            main_logger_->info("Log file: {}", config.file_path);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
    }
}



void SimpleLogger::initializeFromConfig() {
    if (initialized_) {
        if (main_logger_) {
            main_logger_->warn("Logger already initialized, skipping re-initialization");
        }
        return;
    }

    try {
        // 从配置管理器获取日志配置
        auto& config_manager = ConfigManager::getInstance();
        auto logger_config = config_manager.getComponentConfig(ConfigFileType::UTILITY, "logger");
        
        LogSinkConfig sink_config;
        sink_config.console_enabled = logger_config.value("console_enabled", true);
        sink_config.file_enabled = logger_config.value("file_enabled", true);
        sink_config.file_path = logger_config.value("file_path", std::string("logs/gnc.log"));
        sink_config.max_file_size = logger_config.value("max_file_size", 10485760);
        sink_config.max_files = logger_config.value("max_files", 5);
        sink_config.async_enabled = logger_config.value("async_enabled", true);
        
        // 从配置文件获取logger名称
        std::string config_logger_name = config_manager.getConfigValue<std::string>(ConfigFileType::UTILITY, "logger.name", "gnc_main");
        
        // 获取日志级别
        std::string level_str = config_manager.getConfigValue<std::string>(ConfigFileType::UTILITY, "utility.logger.level", "info");
        LogLevel level = LogLevel::INFO; // 默认级别
        
        // 转换字符串到日志级别
        if (level_str == "trace") level = LogLevel::TRACE;
        else if (level_str == "debug") level = LogLevel::DEBUG;
        else if (level_str == "info") level = LogLevel::INFO;
        else if (level_str == "warn") level = LogLevel::WARN;
        else if (level_str == "error") level = LogLevel::ERR;
        else if (level_str == "critical") level = LogLevel::CRITICAL;
        else if (level_str == "off") level = LogLevel::OFF;
        
        // 使用配置初始化日志系统
        initialize(config_logger_name, level, sink_config);
        
        if (main_logger_) {
            main_logger_->info("Logger initialized from config");
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger from config: " << e.what() << std::endl;
        // 使用默认配置作为备用
        initialize("gnc_default");
    }
}



std::shared_ptr<spdlog::logger> SimpleLogger::getMainLogger() {
    if (!initialized_) {
        // 如果未初始化，使用默认配置初始化
        initialize("gnc_default");
    }
    return main_logger_;
}

std::shared_ptr<spdlog::logger> SimpleLogger::getComponentLogger(const std::string& component_name) {
    if (!initialized_) {
        // 如果未初始化，使用默认配置初始化
        initialize("gnc_default");
    }
    
    // 检查是否已存在该组件的日志器
    auto it = component_loggers_.find(component_name);
    if (it != component_loggers_.end()) {
        return it->second;
    }
    
    try {
        // 创建组件日志器，使用相同的输出目标
        std::string logger_name = "gnc." + component_name;
        
        std::shared_ptr<spdlog::logger> component_logger;
        
        // 检查是否使用异步日志
        if (spdlog::thread_pool()) {
            // 创建异步组件日志器
            component_logger = std::make_shared<spdlog::async_logger>(
                logger_name,
                sinks_.begin(),
                sinks_.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            // 创建同步组件日志器
            component_logger = std::make_shared<spdlog::logger>(logger_name, sinks_.begin(), sinks_.end());
        }
        
        // 设置日志级别和格式
        component_logger->set_level(toSpdlogLevel(current_level_));
        component_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
        
        // 注册日志器
        spdlog::register_logger(component_logger);
        
        // 缓存组件日志器
        component_loggers_[component_name] = component_logger;
        
        return component_logger;
        
    } catch (const std::exception& e) {
        if (main_logger_) {
            main_logger_->error("Failed to create component logger for '{}': {}", component_name, e.what());
        }
        // 返回主日志器作为备用
        return main_logger_;
    }
}

void SimpleLogger::setLogLevel(LogLevel level) {
    current_level_ = level;
    auto spdlog_level = toSpdlogLevel(level);
    
    // 设置主日志器级别
    if (main_logger_) {
        main_logger_->set_level(spdlog_level);
    }
    
    // 设置所有组件日志器级别
    for (auto& [name, logger] : component_loggers_) {
        if (logger) {
            logger->set_level(spdlog_level);
        }
    }
    
    if (main_logger_) {
        main_logger_->info("Log level changed to: {}", static_cast<int>(level));
    }
}

void SimpleLogger::flush() {
    if (main_logger_) {
        main_logger_->flush();
    }
    
    for (auto& [name, logger] : component_loggers_) {
        if (logger) {
            logger->flush();
        }
    }
}

void SimpleLogger::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (main_logger_) {
        main_logger_->info("Shutting down GNC Logger");
    }
    
    // 刷新所有日志
    flush();
    
    // 清理组件日志器
    component_loggers_.clear();
    
    // 清理主日志器
    main_logger_.reset();
    
    // 清理输出目标
    sinks_.clear();
    
    // 关闭 spdlog
    spdlog::shutdown();
    
    initialized_ = false;
}

spdlog::level::level_enum SimpleLogger::toSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return spdlog::level::trace;
        case LogLevel::DEBUG:    return spdlog::level::debug;
        case LogLevel::INFO:     return spdlog::level::info;
        case LogLevel::WARN:     return spdlog::level::warn;
        case LogLevel::ERR:      return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF:      return spdlog::level::off;
        default:                 return spdlog::level::info;
    }
}

std::vector<spdlog::sink_ptr> SimpleLogger::createSinks(const LogSinkConfig& config) {
    std::vector<spdlog::sink_ptr> sinks;
    
    try {
        // 创建控制台输出目标
        if (config.console_enabled) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace); // 控制台显示所有级别
            sinks.push_back(console_sink);
        }
        
        // 创建文件输出目标
        if (config.file_enabled) {
            // 确保日志目录存在
            std::filesystem::path log_path(config.file_path);
            std::filesystem::path log_dir = log_path.parent_path();
            
            if (!log_dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(log_dir, ec);
                if (ec) {
                    std::cerr << "Failed to create log directory: " << ec.message() << std::endl;
                }
            }
            
            // 创建轮转文件输出目标
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.file_path, 
                config.max_file_size, 
                config.max_files
            );
            file_sink->set_level(spdlog::level::trace); // 文件记录所有级别
            sinks.push_back(file_sink);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to create log sinks: " << e.what() << std::endl;
    }
    
    return sinks;
}

} // namespace utility
} // namespace components
} // namespace gnc