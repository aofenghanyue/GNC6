/**
 * @file coordination_initializer.hpp
 * @brief 用户自定义的坐标系管理初始化组件
 * 
 * 这个组件继承自SimpleCoordinationInitializer，专门为当前GNC系统
 * 配置所需的坐标变换关系。
 */

#pragma once

#include "gnc/components/utility/simple_coordination_initializer.hpp"

namespace gnc {
namespace components {
namespace custom {

/**
 * @brief 自定义坐标系初始化器
 * 
 * 为当前GNC系统配置必要的坐标变换关系，特别是支持
 * 刚体动力学组件中惯性系到机体系的速度转换。
 */
class CoordinationInitializer : public utility::SimpleCoordinationInitializer {
public:
    /**
     * @brief 构造函数
     */
    CoordinationInitializer(states::VehicleId id, 
                           const std::string& instanceName = "")
        : utility::SimpleCoordinationInitializer(id, instanceName) {
    }

    /**
     * @brief 获取组件类型
     */
    std::string getComponentType() const override {
        return "CoordinationInitializer";
    }

protected:
    /**
     * @brief 注册自定义变换关系
     * 
     * 在这里配置当前GNC系统所需的所有坐标变换关系
     */
    void registerCustomTransforms() override {
        LOG_COMPONENT_DEBUG("Registering custom coordinate transforms");
        
        // 添加惯性系到机体系的动态变换关系
        // 这是支持刚体动力学组件中 SAFE_TRANSFORM_VEC(velocity_, "INERTIAL", "BODY") 的关键
        ADD_DYNAMIC_TRANSFORM("INERTIAL", "BODY",
            [this]() -> Transform {
                try {
                    // 直接在这里读取所需的状态，无限制！
                    auto attitude_quat = this->getStateForCoordination<Quaterniond>(
                        StateId{{1, "Dynamics"}, "attitude_truth_quat"});
                    // 从姿态四元数创建变换矩阵
                    // 注意：这里需要取逆，因为四元数表示的是从机体到惯性的变换
                    // 而我们需要的是从惯性到机体的变换
                    Transform inertial_to_body(attitude_quat);
                    return inertial_to_body.inverse();
                } catch (const std::exception& e) {
                    LOG_COMPONENT_WARN("Failed to compute INERTIAL->BODY transform: {}", e.what());
                    return Transform::Identity(); // 失败时返回单位变换
                }
            }, "Inertial to Body transformation based on attitude quaternion");
            
        // 可以在这里添加更多自定义变换关系
        // 示例：多状态的复杂变换
        /*
        ADD_DYNAMIC_TRANSFORM("NED", "ECEF",
            [this]() -> Transform {
                // 读取多个状态量
                auto latitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "latitude_deg"});
                auto longitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "longitude_deg"});
                auto altitude = this->getStateForCoordination<double>(
                    StateId{{1, "GPS"}, "altitude_m"});
                    
                // 基于多个状态计算复杂变换
                return computeNedToEcefTransform(latitude, longitude, altitude);
            }, "NED to ECEF transformation based on GPS coordinates");
        */
        
        LOG_COMPONENT_DEBUG("Custom coordinate transforms registered successfully");
    }
};

// 注册组件到系统中
static gnc::ComponentRegistrar<CoordinationInitializer> 
    coordination_initializer_registrar("CoordinationInitializer");

} // namespace custom
} // namespace components
} // namespace gnc