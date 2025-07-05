/**
 * @file simple_coordination_initializer.hpp
 * @brief 简化的坐标转换系统初始化器
 * 
 * 这是一个简化版本的坐标转换系统初始化器，专注于核心功能
 */

#pragma once

#include "gnc/coordination/coordination.hpp"
#include "../../core/component_base.hpp"
#include "../../core/component_registrar.hpp"
#include "../../core/state_access.hpp"
#include "../utility/simple_logger.hpp"

namespace gnc {
namespace components {
namespace utility {

/**
 * @brief 简化的坐标转换系统初始化器组件
 * 
 * 这个组件的唯一职责是初始化全局的坐标转换系统
 */
class SimpleCoordinationInitializer : public states::ComponentBase {
public:
    /**
     * @brief 构造函数
     */
    SimpleCoordinationInitializer(states::VehicleId id, 
                                 const std::string& instanceName = "")
        : states::ComponentBase(id, "SimpleCoordinationInitializer", instanceName) {
        
        // 简单的状态输出
        declareOutput<bool>("coordination_initialized", false);
    }

    /**
     * @brief 获取组件类型
     */
    std::string getComponentType() const override {
        return "SimpleCoordinationInitializer";
    }

    /**
     * @brief 初始化方法
     */
    void initialize() override {
        try {
            // 创建状态获取函数
            auto vector_getter = [this](const StateId& state_id) -> const std::vector<double>& {
                return this->getStateForCoordination<std::vector<double>>(state_id);
            };

            // 可选：创建其他类型的获取器
            auto vector3d_getter = [this](const StateId& state_id) -> const Vector3d& {
                return this->getStateForCoordination<Vector3d>(state_id);
            };

            auto quaternion_getter = [this](const StateId& state_id) -> const Quaterniond& {
                return this->getStateForCoordination<Quaterniond>(state_id);
            };

            // 初始化全局变换管理器
            gnc::coordination::SimpleTransformManager::initialize(
                vector_getter, vector3d_getter, quaternion_getter);
            
            LOG_INFO("[SimpleCoordinationInitializer] Coordination system initialized");
            initialization_successful_ = true;

        } catch (const std::exception& e) {
            LOG_ERROR("[SimpleCoordinationInitializer] Initialization failed: {}", e.what());
            initialization_successful_ = false;
        }
    }

    /**
     * @brief 终结方法
     */
    void finalize() override {
        if (initialization_successful_) {
            LOG_INFO("[SimpleCoordinationInitializer] Shutting down coordination system");
            gnc::coordination::SimpleTransformManager::cleanup();
        }
    }

    /**
     * @brief 获取全局变换提供者
     */
    static gnc::coordination::ITransformProvider& getGlobalProvider() {
        return gnc::coordination::SimpleTransformManager::getInstance();
    }

    /**
     * @brief 检查全局提供者是否可用
     */
    static bool isGlobalProviderAvailable() {
        return gnc::coordination::SimpleTransformManager::isInitialized();
    }

    /**
     * @brief 公共的状态获取方法，供坐标转换系统使用
     * @tparam T 状态数据类型
     * @param state_id 状态标识符
     * @return 状态值的常量引用
     */
    template<typename T>
    const T& getStateForCoordination(const StateId& state_id) const {
        return getState<T>(state_id);
    }

protected:
    /**
     * @brief 组件更新实现
     */
    void updateImpl() override {
        setState("coordination_initialized", initialization_successful_);
        
        // 每个仿真步清除缓存以确保动态变换的实时性
        if (initialization_successful_ && gnc::coordination::SimpleTransformManager::isInitialized()) {
            gnc::coordination::SimpleTransformManager::getInstance().clearCache();
        }
    }

private:
    bool initialization_successful_ = false;
};

// 注册组件
static gnc::ComponentRegistrar<SimpleCoordinationInitializer> 
    simple_coordination_initializer_registrar("SimpleCoordinationInitializer");

} // namespace utility
} // namespace components
} // namespace gnc

// ==================== 简化的全局宏定义 ====================

/**
 * @brief 便利宏：获取全局变换提供者
 */
#define GET_GLOBAL_TRANSFORM_PROVIDER() gnc::components::utility::SimpleCoordinationInitializer::getGlobalProvider()

/**
 * @brief 便利宏：检查全局提供者是否可用
 */
#define IS_GLOBAL_PROVIDER_AVAILABLE() gnc::components::utility::SimpleCoordinationInitializer::isGlobalProviderAvailable()