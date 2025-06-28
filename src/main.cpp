#include "gnc/core/state_manager.hpp"
#include "gnc/components/environment/simple_atmosphere.hpp"
#include "gnc/components/effectors/simple_aerodynamics.hpp"
#include "gnc/components/utility/bias_adapter.hpp"
#include "gnc/components/dynamics/rigid_body_dynamics_6dof.hpp"
// 引入其他组件头文件
#include "gnc/components/sensors/ideal_imu_sensor.hpp"
#include "gnc/components/logic/navigation_logic.hpp"
#include "gnc/components/logic/guidance_logic.hpp"
#include "gnc/components/logic/control_logic.hpp"
// 引入日志系统和配置管理器
#include "gnc/components/utility/simple_logger.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include <iostream>

using namespace gnc;
using namespace gnc::components;

int main() {
    try {
        // 初始化多文件配置管理器
        auto& config_manager = gnc::components::utility::ConfigManager::getInstance();
        if (!config_manager.loadConfigs("config/")) {
            std::cerr << "Warning: Failed to load some configuration files, using defaults" << std::endl;
        }
        
        // 从多文件配置初始化日志系统
        gnc::components::utility::SimpleLogger::getInstance().initializeFromConfig("gnc_simulation");
        
        LOG_INFO("=== GNC Meta-Framework Skeleton Simulation Started ===");
        
        StateManager manager;
        const states::VehicleId VEHICLE_1 = 1;
        
        LOG_INFO("Initializing simulation with Vehicle ID: {}", VEHICLE_1);

        // --- 1. 实例化所有组件 ---
        LOG_INFO("Creating simulation components...");
        // 注意：这里用new，因为StateManager接管了所有权
        // 环境
        auto atmosphere = new SimpleAtmosphere(VEHICLE_1);
        LOG_DEBUG("Created SimpleAtmosphere component");
        
        // 效应器
        auto aerodynamics = new SimpleAerodynamics(VEHICLE_1);
        
        // **关键: 实例化一个偏置适配器，拦截真实气动力**
        // 它读取真实气动力，输出一个被乘上1.2（即20%偏差）的"拉偏"后的气动力
        auto aero_biaser = new BiasAdapter(VEHICLE_1, "AeroForceBiasAdapter", 
                                           {{VEHICLE_1, "Aerodynamics"}, "aero_force_truth_N"}, 
                                           "biased_aero_force_N", 1.2);

        // 动力学
        auto dynamics = new RigidBodyDynamics6DoF(VEHICLE_1);

        // 传感器、导航、制导、控制...
        auto imu_sensor = new IdealIMUSensor(VEHICLE_1);
        auto navigation = new PerfectNavigation(VEHICLE_1);
        auto guidance = new GuidanceLogic(VEHICLE_1);
        auto control = new ControlLogic(VEHICLE_1);

        // --- 2. 注册组件到管理器 ---
        LOG_INFO("Registering components to StateManager...");
        // 注册顺序不重要，组件管理器会自动排序
        manager.registerComponent(atmosphere);
        LOG_DEBUG("Registered SimpleAtmosphere component");
        manager.registerComponent(aerodynamics);
        manager.registerComponent(aero_biaser); // 注册拉偏组件
        manager.registerComponent(dynamics);
        manager.registerComponent(imu_sensor);
        manager.registerComponent(navigation);
        manager.registerComponent(guidance);
        manager.registerComponent(control);
        
        // --- 3. 验证依赖并排序 ---
        // 第一次更新前会自动调用
        // manager.validateAndSortComponents(); 

        // --- 4. 运行仿真循环 ---
        const int num_steps = 5;
        LOG_INFO("Starting simulation loop with {} time steps", num_steps);
        
        for (int i = 0; i < num_steps; ++i) {
            LOG_INFO("=== Time Step {} ===", i);
            
            try {
                manager.updateAll();
                LOG_DEBUG("Time step {} completed successfully", i);
            } catch (const std::exception& e) {
                LOG_ERROR("Error in time step {}: {}", i, e.what());
                throw;
            }
        }

        LOG_INFO("=== Simulation Completed Successfully ===");

    } catch (const GncException& e) {
        LOG_CRITICAL("GNC Exception: {}", e.what());
        std::cerr << "ERROR: " << e.what() << std::endl;
        gnc::components::utility::SimpleLogger::getInstance().shutdown();
        return 1;
    } catch (const std::exception& e) {
        LOG_CRITICAL("Standard exception: {}", e.what());
        std::cerr << "A standard exception occurred: " << e.what() << std::endl;
        gnc::components::utility::SimpleLogger::getInstance().shutdown();
        return 1;
    }

    // 正常结束时关闭日志系统
    LOG_INFO("Program terminating normally");
    gnc::components::utility::SimpleLogger::getInstance().shutdown();
    return 0;
}