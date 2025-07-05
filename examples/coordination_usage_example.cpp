/**
 * @file coordination_usage_example.cpp
 * @brief 优化后的坐标管理系统使用示例
 * 
 * 展示如何使用简化后的坐标转换系统
 */

#include "gnc/coordination/coordination.hpp"
#include "gnc/components/utility/simple_coordination_initializer.hpp"

using namespace gnc::states;
using namespace gnc::coordination;
using namespace gnc::components::utility;

/**
 * @brief 自定义坐标系统初始化器示例
 * 
 * 研究人员可以继承SimpleCoordinationInitializer来添加自定义变换关系
 */
class MyCoordinationInitializer : public SimpleCoordinationInitializer {
protected:
    void registerCustomTransforms() override {
        // 示例1：使用便利宏添加传感器到载体的静态变换
        Transform sensor_to_body = Transform::Identity(); // 实际应该是真实的变换矩阵
        ADD_STATIC_TRANSFORM("SENSOR", "BODY", 
            sensor_to_body, "Sensor mounting transform");
        
        // 示例2：简化的动态变换 - 单一状态
        ADD_DYNAMIC_TRANSFORM("INERTIAL", "BODY",
            [this]() -> Transform {
                auto attitude = this->getStateForCoordination<Quaterniond>(
                    StateId{{1, "Dynamics"}, "attitude_truth_quat"});
                return Transform(attitude).inverse();
            }, "Inertial to Body transformation");
            
        // 示例3：复杂的多状态动态变换 - 这就是优化的关键！
        ADD_DYNAMIC_TRANSFORM("NED", "ECEF",
            [this]() -> Transform {
                // 可以读取任意数量的状态！
                auto latitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "latitude_deg"});
                auto longitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "longitude_deg"});
                auto altitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "altitude_m"});
                    
                // 基于多个状态计算复杂变换
                return calculateNedToEcefTransform(latitude, longitude, altitude);
            }, "NED to ECEF based on GPS coordinates");
            
        // 示例4：更复杂的多状态变换
        ADD_DYNAMIC_TRANSFORM("WIND", "BODY",
            [this]() -> Transform {
                // 读取多种类型的状态
                auto aoa_data = this->getStateForCoordination<std::vector<double>>(
                    StateId{{1, "Aerodynamics"}, "angle_of_attack"});
                auto airspeed = this->getStateForCoordination<Vector3d>(
                    StateId{{1, "Sensors"}, "airspeed_vector"});
                auto density = this->getStateForCoordination<double>(
                    StateId{{1, "Environment"}, "air_density"});
                
                // 基于多个不同类型的状态计算风轴变换
                double alpha = aoa_data[0]; // 攻角
                double beta = aoa_data[1];  // 侧滑角
                return computeWindToBodyTransform(alpha, beta, airspeed, density);
            }, "Wind to Body based on aerodynamic parameters");
    }
};

/**
 * @brief 主函数演示新的使用方式
 */
int main() {
    try {
        // 1. 创建自定义初始化器（这通常在配置文件中完成）
        MyCoordinationInitializer initializer(1, "custom_coordination");
        
        // 2. 初始化坐标系统
        initializer.initialize();
        
        // 3. 现在可以使用简化的转换接口
        
        // 示例：转换速度向量从载体坐标系到惯性坐标系
        std::vector<double> velocity_body = {10.0, 0.0, 0.0}; // X轴10m/s
        
        // 方式1：使用安全转换（推荐给研究人员）
        auto velocity_inertial = SAFE_TRANSFORM_VEC(velocity_body, "BODY", "INERTIAL");
        
        std::cout << "Body velocity: [" << velocity_body[0] << ", " 
                  << velocity_body[1] << ", " << velocity_body[2] << "]\n";
        std::cout << "Inertial velocity: [" << velocity_inertial[0] << ", " 
                  << velocity_inertial[1] << ", " << velocity_inertial[2] << "]\n";
        
        // 方式2：使用Vector3d类型
        Vector3d force_body(100.0, 0.0, 0.0);
        auto force_inertial = safeTransformVector(force_body, "BODY", "INERTIAL");
        
        // 方式3：检查变换是否可用
        if (IS_GLOBAL_PROVIDER_AVAILABLE()) {
            auto& provider = GET_GLOBAL_TRANSFORM_PROVIDER();
            if (provider.hasTransform("SENSOR", "INERTIAL")) {
                std::cout << "Sensor to Inertial transform is available\n";
            }
        }
        
        // 4. 清理（通常在程序结束时）
        initializer.finalize();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

/**
 * @brief 研究人员使用指南（优化后）
 * 
 * 优化后的系统为研究人员提供了以下便利：
 * 
 * 1. 极简的日常使用：
 *    auto result = SAFE_TRANSFORM_VEC(data, "FROM", "TO");
 * 
 * 2. 集中的配置管理：
 *    - 继承 SimpleCoordinationInitializer
 *    - 重写 registerCustomTransforms() 方法
 * 
 * 3. 灵活的多状态访问（新特性！）：
 *    ADD_DYNAMIC_TRANSFORM("FROM", "TO",
 *        [this]() -> Transform {
 *            // 可以读取任意数量和类型的状态！
 *            auto state1 = this->getStateForCoordination<Type1>(id1);
 *            auto state2 = this->getStateForCoordination<Type2>(id2);
 *            auto state3 = this->getStateForCoordination<Type3>(id3);
 *            return computeTransform(state1, state2, state3);
 *        }, "description");
 * 
 * 4. 简化的语法：
 *    - 不再需要模板参数
 *    - 不再需要预定义StateId
 *    - 直接在lambda中读取所需状态
 * 
 * 5. 异常安全设计：
 *    - SAFE_TRANSFORM_VEC 宏确保不会因变换失败而崩溃
 *    - 失败时返回原始数据
 * 
 * 6. 预定义坐标系：
 *    - frames::BODY, frames::INERTIAL, frames::NED 等
 *    - 同时支持自定义字符串坐标系名称
 * 
 * 优化亮点：
 * ✅ 移除了模板化的复杂性
 * ✅ 支持多状态读取，无数量限制
 * ✅ 更直观的编程模式
 * ✅ 更短的调用链路
 */