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


namespace gnc {
namespace coordination {

/**
 * @brief 全局简单变换管理器
 * 
 * 提供单例模式的全局访问，直接管理CoordinateSystemRegistry实例
 */
class SimpleTransformManager {
public:
    /**
     * @brief 获取全局实例
     */
    static CoordinateSystemRegistry& getInstance() {
        if (!instance_) {
            throw std::runtime_error("SimpleTransformManager not initialized");
        }
        return *instance_;
    }

    /**
     * @brief 初始化全局实例（无参数）
     */
    static void initialize() {
        if (!instance_) {
            instance_ = std::make_unique<CoordinateSystemRegistry>();
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
    static std::unique_ptr<CoordinateSystemRegistry> instance_;
};

// 静态成员定义
std::unique_ptr<CoordinateSystemRegistry> SimpleTransformManager::instance_ = nullptr;

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