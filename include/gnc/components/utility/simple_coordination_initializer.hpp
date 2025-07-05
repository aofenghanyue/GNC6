/**
 * @file simple_coordination_initializer.hpp
 * @brief 简化的坐标转换系统初始化器
 * 
 * 这是一个优化后的坐标转换系统初始化器，主要功能：
 * - 初始化全局坐标转换系统
 * - 集中管理所有变换关系的注册
 * - 提供简化的用户扩展接口
 * 
 * 优化特点：
 * - 无需复杂的getter配置
 * - 变换关系配置集中化
 * - 用户只需继承并重写registerCustomTransforms()方法
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
 * @brief 优化后的坐标转换系统初始化器组件
 * 
 * 主要职责：
 * - 初始化全局的坐标转换系统
 * - 集中管理所有变换关系的注册
 * - 为用户提供简化的扩展接口
 * 
 * 使用方式：
 * 1. 继承SimpleCoordinationInitializer
 * 2. 重写registerCustomTransforms()方法
 * 3. 使用addDynamicTransform()和addStaticTransform()添加变换
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
            // 1. 简化：只需要无参数初始化
            gnc::coordination::SimpleTransformManager::initialize();
            
            // 2. 注册默认变换关系
            registerDefaultTransforms();
            registerCustomTransforms();
            
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
     * @brief 注册默认变换关系
     */
    virtual void registerDefaultTransforms() {
        auto& registry = gnc::coordination::SimpleTransformManager::getInstance();
        
        // 惯性坐标系到载体坐标系的动态变换
        registry.addDynamicTransform("INERTIAL", "BODY",
            [this]() -> Transform {
                try {
                    // 直接使用组件的状态访问能力
                    auto attitude = this->getStateForCoordination<Quaterniond>(
                        StateId{{1, "Dynamics"}, "attitude_truth_quat"});
                    return Transform(attitude).inverse(); // 从惯性到载体
                } catch (...) {
                    return Transform::Identity(); // 获取失败时返回单位变换
                }
            }, "Inertial to Body transformation");
    }
    
    /**
     * @brief 注册用户自定义变换关系
     * 用户可以重写此方法添加自定义变换
     */
    virtual void registerCustomTransforms() {
        // 空实现，用户可以重写
    }
    
    /**
     * @brief 便利方法：注册动态变换（简化版）
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @param transform_func 变换计算函数（可以读取任意数量的状态）
     * @param description 描述
     */
    void addDynamicTransform(const std::string& from_frame,
                            const std::string& to_frame,
                            std::function<Transform()> transform_func,
                            const std::string& description = "") {
        auto& registry = gnc::coordination::SimpleTransformManager::getInstance();
        registry.addDynamicTransform(from_frame, to_frame, std::move(transform_func), description);
    }
    
    /**
     * @brief 便利方法：注册静态变换
     */
    void addStaticTransform(const std::string& from_frame,
                           const std::string& to_frame,
                           const Transform& transform,
                           const std::string& description = "") {
        auto& registry = gnc::coordination::SimpleTransformManager::getInstance();
        registry.addStaticTransform(from_frame, to_frame, transform, description);
    }

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

/**
 * @brief 便利宏：添加动态变换（用于用户自定义初始化器中）
 */
#define ADD_DYNAMIC_TRANSFORM(from, to, transform_func, desc) \
    addDynamicTransform(from, to, transform_func, desc)

/**
 * @brief 便利宏：添加静态变换（用于用户自定义初始化器中）
 */
#define ADD_STATIC_TRANSFORM(from, to, transform, desc) \
    addStaticTransform(from, to, transform, desc)
