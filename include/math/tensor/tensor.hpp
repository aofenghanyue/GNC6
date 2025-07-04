/**
 * @file tensor.hpp
 * @brief 数学张量库主头文件
 * 
 * 本文件提供了GNC项目中统一的张量类型定义，基于Eigen库构建。
 * 这些类型别名为项目提供了清晰的数学抽象，同时保持了Eigen的高性能。
 * 
 * 主要特性：
 * - 统一的类型别名系统
 * - 基于Eigen的高性能实现
 * - 支持各种常用的数学对象
 * - 为未来扩展预留了架构灵活性
 * 
 * 使用示例：
 * @code
 * #include <math/tensor/tensor.hpp>
 * using namespace gnc::math::tensor;
 * 
 * // 创建向量和矩阵
 * Vector3d position(1.0, 2.0, 3.0);
 * Vector3d velocity(10.0, 0.0, 0.0);
 * Matrix3d rotation_matrix = Matrix3d::Identity();
 * 
 * // 数学运算
 * Vector3d result = rotation_matrix * position;
 * double magnitude = velocity.norm();
 * @endcode
 */

#pragma once

#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace gnc {
namespace math {
namespace tensor {

// ==================== 基础向量类型 ====================

/**
 * @brief 2维双精度浮点向量
 * 用于表示平面上的位置、速度等量
 */
using Vector2d = Eigen::Vector2d;

/**
 * @brief 3维双精度浮点向量
 * 用于表示空间中的位置、速度、加速度、角速度、力等量
 */
using Vector3d = Eigen::Vector3d;

/**
 * @brief 4维双精度浮点向量
 * 用于表示四元数或其他4维量
 */
using Vector4d = Eigen::Vector4d;

/**
 * @brief 6维双精度浮点向量
 * 用于表示6自由度状态量（位置+姿态或力+力矩）
 */
using Vector6d = Eigen::Matrix<double, 6, 1>;

/**
 * @brief 动态大小双精度浮点向量
 * 用于表示任意维度的状态向量
 */
using VectorXd = Eigen::VectorXd;

// ==================== 基础矩阵类型 ====================

/**
 * @brief 2x2双精度浮点矩阵
 * 用于表示平面变换
 */
using Matrix2d = Eigen::Matrix2d;

/**
 * @brief 3x3双精度浮点矩阵
 * 用于表示旋转矩阵、惯性张量等
 */
using Matrix3d = Eigen::Matrix3d;

/**
 * @brief 4x4双精度浮点矩阵
 * 用于表示齐次变换矩阵
 */
using Matrix4d = Eigen::Matrix4d;

/**
 * @brief 6x6双精度浮点矩阵
 * 用于表示6自由度系统矩阵
 */
using Matrix6d = Eigen::Matrix<double, 6, 6>;

/**
 * @brief 动态大小双精度浮点矩阵
 * 用于表示任意大小的矩阵
 */
using MatrixXd = Eigen::MatrixXd;

// ==================== 旋转表示类型 ====================

/**
 * @brief 双精度四元数
 * 用于表示姿态和旋转
 */
using Quaterniond = Eigen::Quaterniond;

/**
 * @brief 角轴表示
 * 用于表示绕任意轴的旋转
 */
using AngleAxisd = Eigen::AngleAxisd;

// ==================== 特殊张量类型 ====================

/**
 * @brief 3x1位置向量
 * 航空航天中常用的位置表示
 */
using Position3d = Vector3d;

/**
 * @brief 3x1速度向量
 * 航空航天中常用的速度表示
 */
using Velocity3d = Vector3d;

/**
 * @brief 3x1加速度向量
 * 航空航天中常用的加速度表示
 */
using Acceleration3d = Vector3d;

/**
 * @brief 3x1角速度向量
 * 航空航天中常用的角速度表示
 */
using AngularVelocity3d = Vector3d;

/**
 * @brief 3x1角加速度向量
 * 航空航天中常用的角加速度表示
 */
using AngularAcceleration3d = Vector3d;

/**
 * @brief 3x1力向量
 * 航空航天中常用的力表示
 */
using Force3d = Vector3d;

/**
 * @brief 3x1力矩向量
 * 航空航天中常用的力矩表示
 */
using Torque3d = Vector3d;

/**
 * @brief 6x1状态向量（位置+姿态）
 * 用于表示刚体的完整位姿状态
 */
using Pose6d = Vector6d;

/**
 * @brief 6x1速度向量（线速度+角速度）
 * 用于表示刚体的完整速度状态
 */
using Twist6d = Vector6d;

/**
 * @brief 6x1力向量（力+力矩）
 * 用于表示作用在刚体上的完整力系
 */
using Wrench6d = Vector6d;

// ==================== 常用常量和工具函数 ====================

/**
 * @brief 张量常量命名空间
 */
namespace constants {
    /// 数值计算精度
    constexpr double EPSILON = 1e-12;
    
    /// 零向量判断阈值
    constexpr double ZERO_THRESHOLD = 1e-10;
    
    /// 单位化阈值
    constexpr double NORMALIZATION_THRESHOLD = 1e-15;
}

/**
 * @brief 张量工具函数命名空间
 */
namespace utils {

/**
 * @brief 检查向量是否为零向量
 * @param vec 要检查的向量
 * @param threshold 阈值
 * @return 是否为零向量
 */
template<typename Derived>
inline bool isZero(const Eigen::MatrixBase<Derived>& vec, 
                   double threshold = constants::ZERO_THRESHOLD) {
    return vec.norm() < threshold;
}

/**
 * @brief 安全归一化向量（避免零向量）
 * @param vec 要归一化的向量
 * @param default_vec 当输入为零向量时的默认值
 * @return 归一化后的向量
 */
template<typename Derived>
inline auto safeNormalize(const Eigen::MatrixBase<Derived>& vec,
                         const Eigen::MatrixBase<Derived>& default_vec) 
    -> typename Derived::PlainObject {
    if (isZero(vec)) {
        return default_vec.normalized();
    }
    return vec.normalized();
}

/**
 * @brief 安全归一化向量（使用默认单位向量）
 * @param vec 要归一化的向量
 * @return 归一化后的向量
 */
inline Vector3d safeNormalize3d(const Vector3d& vec) {
    static const Vector3d default_vec(1.0, 0.0, 0.0);
    return safeNormalize(vec, default_vec);
}

/**
 * @brief 创建反对称矩阵（用于叉积运算）
 * @param vec 3维向量
 * @return 3x3反对称矩阵
 */
inline Matrix3d skewSymmetric(const Vector3d& vec) {
    Matrix3d skew;
    skew << 0.0,    -vec.z(),  vec.y(),
            vec.z(),  0.0,     -vec.x(),
           -vec.y(),  vec.x(),  0.0;
    return skew;
}

/**
 * @brief 从反对称矩阵提取向量
 * @param skew_matrix 3x3反对称矩阵
 * @return 对应的3维向量
 */
inline Vector3d fromSkewSymmetric(const Matrix3d& skew_matrix) {
    return Vector3d(skew_matrix(2, 1), skew_matrix(0, 2), skew_matrix(1, 0));
}

/**
 * @brief 检查矩阵是否为反对称矩阵
 * @param matrix 要检查的矩阵
 * @param tolerance 容差
 * @return 是否为反对称矩阵
 */
inline bool isSkewSymmetric(const Matrix3d& matrix, 
                           double tolerance = constants::EPSILON) {
    return (matrix + matrix.transpose()).norm() < tolerance;
}

/**
 * @brief 创建齐次变换矩阵
 * @param rotation 3x3旋转矩阵
 * @param translation 3x1平移向量
 * @return 4x4齐次变换矩阵
 */
inline Matrix4d homogeneousTransform(const Matrix3d& rotation, 
                                    const Vector3d& translation) {
    Matrix4d transform = Matrix4d::Identity();
    transform.topLeftCorner<3, 3>() = rotation;
    transform.topRightCorner<3, 1>() = translation;
    return transform;
}

/**
 * @brief 从齐次变换矩阵提取旋转部分
 * @param transform 4x4齐次变换矩阵
 * @return 3x3旋转矩阵
 */
inline Matrix3d extractRotation(const Matrix4d& transform) {
    return transform.topLeftCorner<3, 3>();
}

/**
 * @brief 从齐次变换矩阵提取平移部分
 * @param transform 4x4齐次变换矩阵
 * @return 3x1平移向量
 */
inline Vector3d extractTranslation(const Matrix4d& transform) {
    return transform.topRightCorner<3, 1>();
}

} // namespace utils

} // namespace tensor
} // namespace math
} // namespace gnc