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
#include <iostream>

using namespace gnc;
using namespace gnc::components;

int main() {
    try {
        std::cout << "--- GNC Meta-Framework Skeleton Simulation ---" << std::endl;
        
        StateManager manager;
        const states::VehicleId VEHICLE_1 = 1;

        // --- 1. 实例化所有组件 ---
        // 注意：这里用new，因为StateManager接管了所有权
        // 环境
        auto atmosphere = new SimpleAtmosphere(VEHICLE_1);
        
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
        // 注册顺序不重要，组件管理器会自动排序
        manager.registerComponent(atmosphere);
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
        for (int i = 0; i < num_steps; ++i) {
            std::cout << "\n--- Time Step " << i << " ---" << std::endl;
            manager.updateAll();
        }

        std::cout << "\n--- Simulation Finished ---" << std::endl;

    } catch (const GncException& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "A standard exception occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}