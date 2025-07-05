/**
 * @file coordination.hpp
 * @brief 坐标转换系统主头文件
 * 
 * 本文件是坐标转换系统的统一入口，提供最简化的坐标转换接口。
 * 
 * 架构优化说明：
 * - CoordinateSystemRegistry 直接实现 ITransformProvider 接口
 * - SimpleTransformManager 简化为纯粹的单例管理器
 * - 移除了不必要的 SimpleGncTransformProvider 中间层
 * - 变换关系配置集中在初始化组件中
 */

#pragma once

// 直接包含简化版本
#include "simple_coordination.hpp"

// 为了保持向后兼容，重新导出核心类型
namespace gnc {
namespace coordination {

// 核心类型别名
using Vector3d = ::Vector3d;
using Transform = ::Transform;

// 向后兼容的全局provider访问（现在直接返回CoordinateSystemRegistry）
inline ITransformProvider& getGlobalProvider() {
    return SimpleTransformManager::getInstance();
}

inline bool isGlobalProviderAvailable() {
    return SimpleTransformManager::isInitialized();
}

} // namespace coordination
} // namespace gnc