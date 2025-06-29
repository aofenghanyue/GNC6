// main.cpp
#include <iostream>
#include "gnc/core/state_manager.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/core/component_factory.hpp"

// 包含所有组件头文件以确保自注册代码被编译
#include "gnc/components/environment/simple_atmosphere.hpp"
#include "gnc/components/effectors/simple_aerodynamics.hpp"
#include "gnc/components/dynamics/rigid_body_dynamics_6dof.hpp"
#include "gnc/components/sensors/ideal_imu_sensor.hpp"
#include "gnc/components/logic/navigation_logic.hpp"
#include "gnc/components/logic/control_logic.hpp"
#include "gnc/components/logic/phased_guidance_logic.hpp"
#include "gnc/components/logic/guidance_logic.hpp"


using namespace gnc;
using namespace gnc::components;

int main() {
    try {
        // 1. 初始化核心服务（配置会在getInstance时自动加载）
        auto& config_manager = gnc::components::utility::ConfigManager::getInstance();
        
        LOG_INFO("GNC Simulation Framework Initializing...");

        // 2. 创建状态管理器
        gnc::StateManager state_manager;

        // 3. 从配置加载并创建组件
        auto vehicles_config = config_manager.getConfig(utility::ConfigFileType::CORE)["core"]["vehicles"];

        for (const auto& vehicle_config : vehicles_config) {
            gnc::states::VehicleId vehicle_id = vehicle_config["id"].get<gnc::states::VehicleId>();
            LOG_INFO("Loading components for Vehicle ID: {}", vehicle_id);

            for (const auto& comp_config : vehicle_config["components"]) {
                std::string type_str;
                std::string instance_name;
                
                if (comp_config.is_string()) {
                    // 简单字符串格式（向后兼容）
                    type_str = comp_config.get<std::string>();
                    LOG_DEBUG("Creating component of type: {}", type_str.c_str());
                } else if (comp_config.is_object()) {
                    // 对象格式（支持自定义实例名称）
                    type_str = comp_config["type"].get<std::string>();
                    if (comp_config.contains("name")) {
                        instance_name = comp_config["name"].get<std::string>();
                        LOG_DEBUG("Creating component of type: {} with name: {}", type_str.c_str(), instance_name.c_str());
                    } else {
                        LOG_DEBUG("Creating component of type: {}", type_str.c_str());
                    }
                }
                
                // 使用工厂创建组件实例
                gnc::states::ComponentBase* component = gnc::ComponentFactory::getInstance().createComponent(type_str, vehicle_id, instance_name);
                
                // 注册到状态管理器
                state_manager.registerComponent(component);
            }
        }

        // 4. 验证依赖关系并启动仿真循环
        state_manager.validateAndSortComponents();

        // TODO 我们的GNC框架缺少最重要的一个系统：时间管理系统。无论是飞行器的飞行逻辑，仿真器的调度，组件的更新，数据的记录等，都需要使用到时间系统，当然，我们这是纯仿真框架，不考虑实时系统。需要在core文件夹下创建时间管理系统。
        LOG_INFO("Starting simulation loop...");
         for (int i = 0; i < 10; ++i) { // 示例循环
             state_manager.updateAll();
         }
         LOG_INFO("Simulation loop finished.");

    } catch (const std::exception& e) {
        LOG_CRITICAL("An unhandled exception occurred: {}", e.what());
        return 1;
    }

    // 正常结束时关闭日志系统
    LOG_INFO("Program terminating normally");
    gnc::components::utility::SimpleLogger::getInstance().shutdown();
    return 0;
}