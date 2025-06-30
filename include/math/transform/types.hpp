#pragma once

#include <Eigen/Dense>
#include <memory>

namespace gnc {
namespace math {
namespace transform {

// 基础类型定义
using Vector3 = Eigen::Vector3d;
using Vector4 = Eigen::Vector4d;
using Matrix3 = Eigen::Matrix3d;
using Quaternion = Eigen::Quaterniond;

/**
 * @brief 欧拉角旋转序列
 * 
 * 定义三次旋转的轴顺序，每种序列对应不同的旋转组合方式
 */
enum class EulerSequence {
    ZYX,  ///< Z-Y-X 序列（最常用）
    XYZ,  ///< X-Y-Z 序列
    YZX,  ///< Y-Z-X 序列
    ZXY,  ///< Z-X-Y 序列
    XZY,  ///< X-Z-Y 序列
    YXZ   ///< Y-X-Z 序列
};

/**
 * @brief 数学常量
 */
namespace constants {
    constexpr double EPSILON = 1e-9;
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;
    constexpr double HALF_PI = PI / 2.0;
    constexpr double DEG_TO_RAD = PI / 180.0;
    constexpr double RAD_TO_DEG = 180.0 / PI;
    constexpr double ROTATION_TOLERANCE = 1e-6; ///< 旋转矩阵验证容差
    constexpr double QUATERNION_TOLERANCE = 1e-9; ///< 四元数归一化容差
}

} // namespace transform
} // namespace math
} // namespace gnc