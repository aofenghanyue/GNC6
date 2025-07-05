/**
 * @file math.hpp
 * @brief 数学库统一接口
 * 
 * 提供基础数学类型的全局别名，直接包含即可使用
 */

#pragma once

#include <Eigen/Dense>
#include "transform/transform.hpp"

// ==================== 全局数学类型别名 ====================

// 基础向量和矩阵类型
using Vector3d = Eigen::Vector3d;
using Vector4d = Eigen::Vector4d;
using Quaterniond = Eigen::Quaterniond;
using Matrix3d = Eigen::Matrix3d;
using Matrix4d = Eigen::Matrix4d;

// 坐标变换类型
using Transform = gnc::math::transform::Transform;

// 导出transform库的枚举到全局
using EulerSequence = gnc::math::transform::EulerSequence;

// ==================== 全局数学常量 ====================
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double HALF_PI = PI / 2.0;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;
constexpr double EPSILON = 1e-9;
constexpr double ROTATION_TOLERANCE = 1e-6;
constexpr double QUATERNION_TOLERANCE = 1e-9;