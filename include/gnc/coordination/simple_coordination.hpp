/**
 * @file simple_coordination.hpp
 * @brief 精简的坐标转换系统 - 统一入口
 * 
 * 基于原设计文档的简化版本，保留图论算法核心，简化provider层次
 */

#pragma once

// 核心组件
#include "frame_identifier.hpp"
#include "itransform_provider.hpp"
#include "coordinate_system_registry.hpp"
#include "../../math/math.hpp"
#include "../core/state_manager.hpp"
#include "../core/state_access.hpp"
#include "gnc/components/utility/simple_logger.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>

/**
 * @brief 配对哈希函数
 */
struct PairHash {
    std::size_t operator()(const std::pair<gnc::coordination::FrameIdentifier, gnc::coordination::FrameIdentifier>& pair) const {
        auto h1 = std::hash<std::string>{}(pair.first);
        auto h2 = std::hash<std::string>{}(pair.second);
        return h1 ^ (h2 << 1);
    }
};

namespace gnc {
namespace coordination {

/**
 * @brief 状态获取函数类型
 */
template<typename T>
using StateGetterFunc = std::function<const T&(const StateId&)>;

/**
 * @brief 简化的GNC变换提供者
 * 
 * 这是ITransformProvider的唯一实现，使用状态获取函数获取状态
 */
class SimpleGncTransformProvider : public ITransformProvider {
public:
    /**
     * @brief 构造函数
     * @param vector_getter 获取vector<double>状态的函数
     * @param vector3d_getter 获取Vector3d状态的函数
     * @param quaternion_getter 获取Quaterniond状态的函数
     */
    explicit SimpleGncTransformProvider(
        StateGetterFunc<std::vector<double>> vector_getter,
        StateGetterFunc<Vector3d> vector3d_getter = nullptr,
        StateGetterFunc<Quaterniond> quaternion_getter = nullptr) 
        : vector_getter_(std::move(vector_getter)),
          vector3d_getter_(std::move(vector3d_getter)),
          quaternion_getter_(std::move(quaternion_getter)) {
        initializeTransforms();
    }

    // ITransformProvider 接口实现
    Transform getTransform(
        const FrameIdentifier& from_frame,
        const FrameIdentifier& to_frame) const override {
        
        // 相同坐标系直接返回
        if (validation::areFramesEqual(from_frame, to_frame)) {
            return Transform::Identity();
        }

        // 检查缓存
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto cache_key = std::make_pair(from_frame, to_frame);
        auto it = transform_cache_.find(cache_key);
        if (it != transform_cache_.end()) {
            return it->second;
        }

        // 使用图论算法查找路径
        auto path = registry_.findTransformPath(from_frame, to_frame);
        if (!path) {
            throw TransformNotFoundError(from_frame, to_frame, "No transformation path found");
        }

        // 计算路径总变换
        auto result = path->computeTransform();

        // 缓存结果
        transform_cache_[cache_key] = result;
        return result;
    }

    bool hasTransform(const FrameIdentifier& from_frame,
                     const FrameIdentifier& to_frame) const override {
        return registry_.hasTransformPath(from_frame, to_frame);
    }

    FrameIdentifierSet getSupportedFrames() const override {
        return registry_.getRegisteredFrames();
    }

    bool isFrameSupported(const FrameIdentifier& frame_id) const override {
        auto frames = getSupportedFrames();
        return frames.find(frame_id) != frames.end();
    }

    void clearCache() const override {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        transform_cache_.clear();
    }

    std::string getProviderInfo() const override {
        return "Simple GNC Transform Provider";
    }

private:
    StateGetterFunc<std::vector<double>> vector_getter_;
    StateGetterFunc<Vector3d> vector3d_getter_;
    StateGetterFunc<Quaterniond> quaternion_getter_;
    CoordinateSystemRegistry registry_;
    mutable std::unordered_map<std::pair<FrameIdentifier, FrameIdentifier>, 
                              Transform,
                              PairHash> transform_cache_;
    mutable std::mutex cache_mutex_;

    /**
     * @brief 初始化所有变换关系
     */
    void initializeTransforms() {
        // 惯性坐标系到载体坐标系的动态变换
        registry_.addDynamicTransform("INERTIAL", "BODY",
            [this]() -> Transform {
                try {
                    // 使用vector_getter获取四元数数据（存储为std::vector<double>）
                    if (quaternion_getter_) {
                        auto attitude = quaternion_getter_(
                            StateId{{1, "Dynamics"}, "attitude_truth_quat"});
                        
                        Transform transform(attitude);
                        return transform.inverse(); // 从惯性到载体
                        
                    }
                } catch (...) {
                    // 获取失败时返回单位变换
                }
                return Transform::Identity();
            },
            "Inertial to Body transformation");

        // 可以在这里添加更多变换关系
        // 例如使用不同类型的状态获取器：
        // registry_.addDynamicTransform("NED", "INERTIAL",
        //     [this]() -> Transform {
        //         if (vector3d_getter_) {
        //             auto position = vector3d_getter_(
        //                 StateId{{1, "GPS"}, "position_ned"});
        //             // 基于位置计算变换...
        //         }
        //         return Transform::Identity();
        //     });
    }
};

/**
 * @brief 全局简单变换管理器
 * 
 * 提供单例模式的全局访问
 */
class SimpleTransformManager {
public:
    /**
     * @brief 获取全局实例
     */
    static SimpleGncTransformProvider& getInstance() {
        if (!instance_) {
            throw std::runtime_error("SimpleTransformManager not initialized");
        }
        return *instance_;
    }

    /**
     * @brief 初始化全局实例
     * @param vector_getter 获取vector<double>状态的函数
     * @param vector3d_getter 获取Vector3d状态的函数
     * @param quaternion_getter 获取Quaterniond状态的函数
     */
    static void initialize(
        StateGetterFunc<std::vector<double>> vector_getter,
        StateGetterFunc<Vector3d> vector3d_getter = nullptr,
        StateGetterFunc<Quaterniond> quaternion_getter = nullptr) {
        if (!instance_) {
            instance_ = std::make_unique<SimpleGncTransformProvider>(
                std::move(vector_getter), std::move(vector3d_getter), std::move(quaternion_getter));
        }
    }

    /**
     * @brief 清理全局实例
     */
    static void cleanup() {
        instance_.reset();
    }

    /**
     * @brief 检查是否已初始化
     */
    static bool isInitialized() {
        return instance_ != nullptr;
    }

private:
    static std::unique_ptr<SimpleGncTransformProvider> instance_;
};

// 静态成员定义
std::unique_ptr<SimpleGncTransformProvider> SimpleTransformManager::instance_ = nullptr;

// ==================== 核心转换接口 ====================

/**
 * @brief 向量坐标转换（std::vector版本）
 */
inline std::vector<double> transformVector(const std::vector<double>& vec_data,
                                          const FrameIdentifier& from_frame,
                                          const FrameIdentifier& to_frame) {
    if (vec_data.size() < 3) {
        throw std::invalid_argument("Vector must have at least 3 elements");
    }
    
    if (validation::areFramesEqual(from_frame, to_frame)) {
        return vec_data;
    }
    
    auto& provider = SimpleTransformManager::getInstance();
    auto transform = provider.getTransform(from_frame, to_frame);
    
    Vector3d vec(vec_data[0], vec_data[1], vec_data[2]);
    auto transformed = transform * vec;
    
    return {transformed.x(), transformed.y(), transformed.z()};
}

/**
 * @brief 向量坐标转换（Vector3d版本）
 */
inline Vector3d transformVector(const Vector3d& vec_data,
                               const FrameIdentifier& from_frame,
                               const FrameIdentifier& to_frame) {
    if (validation::areFramesEqual(from_frame, to_frame)) {
        return vec_data;
    }
    
    auto& provider = SimpleTransformManager::getInstance();
    auto transform = provider.getTransform(from_frame, to_frame);
    auto transformed = transform * vec_data;
    
    return transformed;
}

/**
 * @brief 安全的向量坐标转换（std::vector版本）
 */
inline std::vector<double> safeTransformVector(const std::vector<double>& vec_data,
                                               const FrameIdentifier& from_frame,
                                               const FrameIdentifier& to_frame) {
    try {
        if (!SimpleTransformManager::isInitialized()) {
            return vec_data;
        }
        return transformVector(vec_data, from_frame, to_frame);
    } catch (...) {
        return vec_data;
    }
}

/**
 * @brief 安全的向量坐标转换（Vector3d版本）
 */
inline Vector3d safeTransformVector(const Vector3d& vec_data,
                                   const FrameIdentifier& from_frame,
                                   const FrameIdentifier& to_frame) {
    try {
        if (!SimpleTransformManager::isInitialized()) {
            return vec_data;
        }
        return transformVector(vec_data, from_frame, to_frame);
    } catch (...) {
        LOG_WARN("Failed to transform vector from {} to {}", from_frame.c_str(), to_frame.c_str());
        return vec_data;
    }
}

} // namespace coordination
} // namespace gnc

// ==================== 简化宏定义 ====================

/**
 * @brief 安全转换std::vector的宏
 */
#define SAFE_TRANSFORM_VEC(vec, from, to) gnc::coordination::safeTransformVector(vec, from, to)

/**
 * @brief 转换std::vector的宏（可能抛出异常）
 */
#define TRANSFORM_VEC(vec, from, to) gnc::coordination::transformVector(vec, from, to)