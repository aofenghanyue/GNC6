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
#include "gnc/components/logic/guidance_logic.hpp"
#include "gnc/components/logic/control_logic.hpp"

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

            for (const auto& comp_type : vehicle_config["components"]) {
                std::string type_str = comp_type.get<std::string>();
                LOG_DEBUG("Creating component of type: {}", type_str.c_str());
                
                // 使用工厂创建组件实例
                gnc::states::ComponentBase* component = gnc::ComponentFactory::getInstance().createComponent(type_str, vehicle_id);
                
                // 注册到状态管理器
                state_manager.registerComponent(component);
            }
        }

        // 4. 验证依赖关系并启动仿真循环
        state_manager.validateAndSortComponents();

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