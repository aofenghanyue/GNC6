/**
 * @file coordination.hpp
 * @brief 坐标转换系统主头文件
 * 
 * 本文件是坐标转换系统的统一入口，提供最简化的坐标转换接口。
 */

#pragma once

// 直接包含简化版本
#include "simple_coordination.hpp"

// 为了保持向后兼容，重新导出核心类型
namespace gnc {
namespace coordination {

// 核心类型别名
using Vector3d = gnc::math::tensor::Vector3d;
using Transform = gnc::math::transform::Transform;

// 向后兼容的全局provider访问
inline ITransformProvider& getGlobalProvider() {
    return SimpleTransformManager::getInstance();
}

inline bool isGlobalProviderAvailable() {
    return SimpleTransformManager::isInitialized();
}

} // namespace coordination
} // namespace gnc