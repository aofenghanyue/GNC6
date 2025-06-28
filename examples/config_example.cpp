/**
 * @file config_example.cpp
 * @brief 配置管理器使用示例
 * 
 * 本示例演示如何使用 ConfigManager 来管理分离的配置文件
 */

#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include <iostream>
#include <iomanip>

using namespace gnc::components::utility;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printConfigSection(const std::string& section_name, const nlohmann::json& config) {
    std::cout << "\n[" << section_name << "]" << std::endl;
    std::cout << config.dump(2) << std::endl;
}

int main() {
    try {
        printSeparator("GNC 配置管理器示例");
        
        // 1. 初始化配置管理器
        std::cout << "\n1. 初始化配置管理器..." << std::endl;
        auto& config_manager = ConfigManager::getInstance();
        
        if (config_manager.loadConfigs("config/")) {
            std::cout << "   ✓ 配置文件加载成功" << std::endl;
        } else {
            std::cout << "   ⚠ 部分配置文件加载失败，使用默认配置" << std::endl;
        }
        
        // 2. 从配置初始化日志系统
        std::cout << "\n2. 从配置初始化日志系统..." << std::endl;
        SimpleLogger::getInstance().initializeFromConfig();
        std::cout << "   ✓ 日志系统初始化完成" << std::endl;
        
        LOG_INFO("配置管理器示例开始");
        
        // 3. 获取和显示各类型配置
        printSeparator("配置文件内容展示");
        
        // 核心配置
        auto core_config = config_manager.getConfig(ConfigFileType::CORE);
        printConfigSection("核心配置 (core.json)", core_config);
        
        // 动力学配置
        auto dynamics_config = config_manager.getConfig(ConfigFileType::DYNAMICS);
        printConfigSection("动力学配置 (dynamics.json)", dynamics_config);
        
        // 环境配置
        auto environment_config = config_manager.getConfig(ConfigFileType::ENVIRONMENT);
        printConfigSection("环境配置 (environment.json)", environment_config);
        
        // 效应器配置
        auto effectors_config = config_manager.getConfig(ConfigFileType::EFFECTORS);
        printConfigSection("效应器配置 (effectors.json)", effectors_config);
        
        // 逻辑配置
        auto logic_config = config_manager.getConfig(ConfigFileType::LOGIC);
        printConfigSection("逻辑配置 (logic.json)", logic_config);
        
        // 传感器配置
        auto sensors_config = config_manager.getConfig(ConfigFileType::SENSORS);
        printConfigSection("传感器配置 (sensors.json)", sensors_config);
        
        // 工具配置
        auto utility_config = config_manager.getConfig(ConfigFileType::UTILITY);
        printConfigSection("工具配置 (utility.json)", utility_config);
        
        // 4. 获取特定组件配置
        printSeparator("特定组件配置获取");
        
        // 获取导航组件配置
        auto nav_config = config_manager.getComponentConfig(ConfigFileType::LOGIC, "navigation");
        std::cout << "\n导航组件配置:" << std::endl;
        std::cout << "  - 启用状态: " << nav_config.value("enabled", false) << std::endl;
        std::cout << "  - 更新频率: " << nav_config.value("update_frequency", 0.0) << " Hz" << std::endl;
        std::cout << "  - 滤波器类型: " << nav_config.value("filter_type", "unknown") << std::endl;
        
        // 获取控制组件配置
        auto control_config = config_manager.getComponentConfig(ConfigFileType::LOGIC, "control");
        std::cout << "\n控制组件配置:" << std::endl;
        std::cout << "  - 启用状态: " << control_config.value("enabled", false) << std::endl;
        std::cout << "  - 更新频率: " << control_config.value("update_frequency", 0.0) << " Hz" << std::endl;
        if (control_config.contains("pid_gains")) {
            auto pid_gains = control_config["pid_gains"];
            if (pid_gains.contains("position")) {
                auto pos_gains = pid_gains["position"];
                std::cout << "  - PID增益 (位置): Kp=" << pos_gains.value("kp", 0.0) 
                         << ", Ki=" << pos_gains.value("ki", 0.0) 
                         << ", Kd=" << pos_gains.value("kd", 0.0) << std::endl;
            }
        }
        
        // 获取IMU传感器配置
        auto imu_config = config_manager.getComponentConfig(ConfigFileType::SENSORS, "imu");
        std::cout << "\nIMU传感器配置:" << std::endl;
        std::cout << "  - 启用状态: " << imu_config.value("enabled", false) << std::endl;
        std::cout << "  - 更新频率: " << imu_config.value("update_frequency", 0.0) << " Hz" << std::endl;
        std::cout << "  - 陀螺仪噪声标准差: " << imu_config.value("gyro_noise_std", 0.0) << std::endl;
        std::cout << "  - 加速度计噪声标准差: " << imu_config.value("accel_noise_std", 0.0) << std::endl;
        
        // 5. 获取全局配置
        printSeparator("全局配置参数");
        
        auto global_config = config_manager.getGlobalConfig();
        std::cout << "\n全局仿真参数:" << std::endl;
        std::cout << "  - 仿真时间步长: " << global_config.value("simulation_time_step", 0.0) << " s" << std::endl;
        std::cout << "  - 最大仿真时间: " << global_config.value("max_simulation_time", 0.0) << " s" << std::endl;
        std::cout << "  - 实时因子: " << global_config.value("real_time_factor", 0.0) << std::endl;
        
        // 6. 使用模板方法获取配置值
        printSeparator("模板方法获取配置值");
        
        double time_step = config_manager.getConfigValue<double>(ConfigFileType::CORE, "global.simulation_time_step", 0.01);
        bool console_enabled = config_manager.getConfigValue<bool>(ConfigFileType::CORE, "logger.console_enabled", true);
        std::string log_level = config_manager.getConfigValue<std::string>(ConfigFileType::CORE, "logger.level", "info");
        
        std::cout << "\n使用模板方法获取的配置值:" << std::endl;
        std::cout << "  - 仿真时间步长: " << time_step << " s" << std::endl;
        std::cout << "  - 控制台日志启用: " << (console_enabled ? "是" : "否") << std::endl;
        std::cout << "  - 日志级别: " << log_level << std::endl;
        
        // 7. 修改配置值
        printSeparator("配置值修改");
        
        std::cout << "\n修改配置值..." << std::endl;
        
        // 修改仿真时间步长
        config_manager.setConfigValue(ConfigFileType::CORE, "global.simulation_time_step", 0.005);
        std::cout << "  ✓ 修改仿真时间步长为 0.005s" << std::endl;
        
        // 修改导航更新频率
        config_manager.setConfigValue(ConfigFileType::LOGIC, "components.navigation.update_frequency", 200.0);
        std::cout << "  ✓ 修改导航更新频率为 200Hz" << std::endl;
        
        // 验证修改
        double new_time_step = config_manager.getConfigValue<double>(ConfigFileType::CORE, "global.simulation_time_step", 0.01);
        double new_nav_freq = config_manager.getConfigValue<double>(ConfigFileType::LOGIC, "components.navigation.update_frequency", 100.0);
        
        std::cout << "\n修改后的配置值:" << std::endl;
        std::cout << "  - 新的仿真时间步长: " << new_time_step << " s" << std::endl;
        std::cout << "  - 新的导航更新频率: " << new_nav_freq << " Hz" << std::endl;
        
        // 8. 保存配置
        printSeparator("保存配置");
        
        std::cout << "\n保存修改后的配置..." << std::endl;
        if (config_manager.saveConfigs()) {
            std::cout << "  ✓ 所有配置文件保存成功" << std::endl;
        } else {
            std::cout << "  ✗ 配置文件保存失败" << std::endl;
        }
        
        // 9. 配置验证
        printSeparator("配置验证");
        
        std::cout << "\n验证配置文件..." << std::endl;
        if (config_manager.validateConfigs()) {
            std::cout << "  ✓ 所有配置文件验证通过" << std::endl;
        } else {
            std::cout << "  ✗ 配置文件验证失败" << std::endl;
        }
        
        // 10. 配置重载
        printSeparator("配置重载");
        
        std::cout << "\n重新加载配置文件..." << std::endl;
        if (config_manager.reloadConfigs()) {
            std::cout << "  ✓ 所有配置文件重载成功" << std::endl;
        } else {
            std::cout << "  ✗ 配置文件重载失败" << std::endl;
        }
        
        LOG_INFO("多文件配置管理器示例完成");
        
        printSeparator("示例完成");
        std::cout << "\n多文件配置管理器示例运行完成！" << std::endl;
        std::cout << "\n配置文件已按组件类型分离：" << std::endl;
        std::cout << "  - core.json: 核心系统配置（日志、全局参数）" << std::endl;
        std::cout << "  - dynamics.json: 动力学组件配置" << std::endl;
        std::cout << "  - environment.json: 环境组件配置" << std::endl;
        std::cout << "  - effectors.json: 效应器组件配置" << std::endl;
        std::cout << "  - logic.json: 逻辑组件配置（导航、制导、控制）" << std::endl;
        std::cout << "  - sensors.json: 传感器组件配置" << std::endl;
        std::cout << "  - utility.json: 工具组件配置" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n错误: " << e.what() << std::endl;
        return 1;
    }
    
    // 关闭日志系统
    SimpleLogger::getInstance().shutdown();
    return 0;
}