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
            // 创建状态获取函数，通过组件的stateAccess_访问状态
            auto state_getter = [this](const StateId& state_id) -> std::any {
                try {
                    // 通过组件的getState方法访问状态
                    return this->getState<std::vector<double>>(state_id);
                } catch (...) {
                    // 如果获取失败，返回空值
                    return std::any();
                }
            };

            // 初始化全局变换管理器
            gnc::coordination::SimpleTransformManager::initialize(state_getter);
            
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