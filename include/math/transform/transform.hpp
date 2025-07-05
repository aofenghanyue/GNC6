#pragma once

/**
 * @file transform.hpp
 * @brief 数学变换库主头文件
 * 
 * 这个头文件包含了完整的3D旋转变换库，提供统一的Transform类。
 * 内部使用四元数进行高效计算，支持多种输入和输出格式。
 * 
 * 主要特性：
 * - 统一的Transform类接口
 * - 内部四元数优化计算
 * - 支持多种输入格式（四元数、矩阵、欧拉角）
 * - 支持多种输出格式转换
 * - 数值稳定的算法实现
 * - 高效的计算性能
 * 
 * 使用示例：
 * @code
 * #include <math/transform/transform.hpp>
 * using namespace gnc::math::transform;
 * 
 * // 创建变换（多种方式）
 * auto transform1 = Transform::Identity();
 * auto transform2 = Transform::FromEuler(0.1, 0.2, 0.3);
 * auto transform3 = Transform::RotationX(0.5);
 * 
 * // 变换向量
 * Vector3 result = transform2 * Vector3(1, 0, 0);
 * 
 * // 变换组合
 * auto combined = transform1 * transform2 * transform3;
 * 
 * // 表示形式转换
 * Quaternion quat = transform2.asQuaternion();
 * Matrix3 matrix = transform2.asMatrix();
 * Vector3 euler = transform2.asEuler(EulerSequence::ZYX);
 * @endcode
 */

// 基础类型和常量
#include <Eigen/Dense>

namespace gnc {
namespace math {
namespace transform {

// 内部使用的类型别名
using Vector3 = Eigen::Vector3d;
using Vector4 = Eigen::Vector4d;
using Matrix3 = Eigen::Matrix3d;
using Quaternion = Eigen::Quaterniond;

// 内部使用的常量
namespace constants {
    constexpr double EPSILON = 1e-9;
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;
    constexpr double HALF_PI = PI / 2.0;
    constexpr double DEG_TO_RAD = PI / 180.0;
    constexpr double RAD_TO_DEG = 180.0 / PI;
    constexpr double ROTATION_TOLERANCE = 1e-6;
    constexpr double QUATERNION_TOLERANCE = 1e-9;
}

// 欧拉角旋转序列（内部使用）
enum class EulerSequence {
    ZYX, XYZ, YZX, ZXY, XZY, YXZ
};

/**
 * @brief 统一的变换类
 * 
 * 这个类提供了一个统一的接口来处理所有类型的旋转变换。
 * 内部使用四元数进行计算以获得最佳性能和数值稳定性，
 * 但可以接受和输出任何表示形式（四元数、矩阵、欧拉角）。
 * 
 * 设计理念：
 * - 统一接口：用户只需要使用一个Transform类
 * - 高性能：内部使用四元数计算
 * - 灵活输入：支持多种构造方式
 * - 灵活输出：支持多种输出格式
 * - 链式操作：支持变换组合
 */
class Transform {
private:
    Quaternion quat_;  // 内部统一使用四元数存储
    
public:
    // ==================== 构造函数 ====================
    
    /**
     * @brief 默认构造函数，创建单位变换
     */
    Transform() : quat_(1.0, 0.0, 0.0, 0.0) {}
    
    /**
     * @brief 从四元数构造
     */
    Transform(const Quaternion& q) : quat_(q.normalized()) {}
    
    /**
     * @brief 从四元数分量构造
     */
    Transform(double w, double x, double y, double z) 
        : quat_(w, x, y, z) {
        quat_.normalize();
    }
    
    /**
     * @brief 从旋转矩阵构造
     */
    Transform(const Matrix3& matrix) {
        quat_ = Quaternion(matrix).normalized();
    }
    
    /**
     * @brief 从欧拉角构造
     */
    Transform(double angle1, double angle2, double angle3, 
             EulerSequence sequence = EulerSequence::XYZ) {
        quat_ = eulerToQuaternion(angle1, angle2, angle3, sequence).normalized();
    }
    
    /**
     * @brief 从欧拉角向量构造
     */
    Transform(const Vector3& euler_angles, 
             EulerSequence sequence = EulerSequence::XYZ) 
        : Transform(euler_angles.x(), euler_angles.y(), euler_angles.z(), sequence) {}
    
    // ==================== 静态工厂方法 ====================
    
    /**
     * @brief 创建单位变换
     */
    static Transform Identity() {
        return Transform();
    }
    
    /**
     * @brief 从四元数创建
     */
    static Transform FromQuaternion(const Quaternion& q) {
        return Transform(q);
    }
    
    /**
     * @brief 从四元数分量创建
     */
    static Transform FromQuaternion(double w, double x, double y, double z) {
        return Transform(w, x, y, z);
    }
    
    /**
     * @brief 从旋转矩阵创建
     */
    static Transform FromMatrix(const Matrix3& matrix) {
        return Transform(matrix);
    }
    
    /**
     * @brief 从欧拉角创建
     */
    static Transform FromEuler(double angle1, double angle2, double angle3, 
                              EulerSequence sequence = EulerSequence::XYZ) {
        return Transform(angle1, angle2, angle3, sequence);
    }
    
    /**
     * @brief 从欧拉角向量创建
     */
    static Transform FromEuler(const Vector3& euler_angles, 
                              EulerSequence sequence = EulerSequence::XYZ) {
        return Transform(euler_angles, sequence);
    }
    
    /**
     * @brief 绕X轴旋转
     */
    static Transform RotationX(double angle) {
        return Transform(angle, 0.0, 0.0, EulerSequence::XYZ);
    }
    
    /**
     * @brief 绕Y轴旋转
     */
    static Transform RotationY(double angle) {
        return Transform(0.0, angle, 0.0, EulerSequence::XYZ);
    }
    
    /**
     * @brief 绕Z轴旋转
     */
    static Transform RotationZ(double angle) {
        return Transform(0.0, 0.0, angle, EulerSequence::XYZ);
    }
    
    /**
     * @brief 绕任意轴旋转
     */
    static Transform RotationAxis(const Vector3& axis, double angle) {
        Vector3 normalized_axis = axis.normalized();
        double half_angle = angle * 0.5;
        double sin_half = std::sin(half_angle);
        return Transform(
            std::cos(half_angle),
            normalized_axis.x() * sin_half,
            normalized_axis.y() * sin_half,
            normalized_axis.z() * sin_half
        );
    }
    
    // ==================== 基本变换操作 ====================
    
    /**
     * @brief 获取逆变换
     */
    Transform inverse() const {
        return Transform(quat_.conjugate());
    }
    
    /**
     * @brief 变换组合（this * other）
     */
    Transform compose(const Transform& other) const {
        return Transform(quat_ * other.quat_);
    }
    
    /**
     * @brief 变换向量
     */
    Vector3 transform(const Vector3& vector) const {
        return quat_ * vector;
    }
    
    /**
     * @brief 变换组合运算符
     */
    Transform operator*(const Transform& other) const {
        return compose(other);
    }
    
    /**
     * @brief 变换向量运算符
     */
    Vector3 operator*(const Vector3& vector) const {
        return transform(vector);
    }
    
    /**
     * @brief 复合赋值运算符
     */
    Transform& operator*=(const Transform& other) {
        quat_ = quat_ * other.quat_;
        return *this;
    }
    
    // ==================== 输出转换 ====================
    
    /**
     * @brief 转换为四元数
     */
    Quaternion asQuaternion() const {
        return quat_;
    }
    
    /**
     * @brief 转换为旋转矩阵
     */
    Matrix3 asMatrix() const {
        return quat_.toRotationMatrix();
    }
    
    /**
     * @brief 转换为欧拉角
     */
    Vector3 asEuler(EulerSequence sequence = EulerSequence::XYZ) const {
        return quaternionToEuler(quat_, sequence);
    }
    
    /**
     * @brief 获取欧拉角的单个分量
     */
    double getAngle1(EulerSequence sequence = EulerSequence::XYZ) const {
        return asEuler(sequence).x();
    }
    
    double getAngle2(EulerSequence sequence = EulerSequence::XYZ) const {
        return asEuler(sequence).y();
    }
    
    double getAngle3(EulerSequence sequence = EulerSequence::XYZ) const {
        return asEuler(sequence).z();
    }
    
    // ==================== 实用方法 ====================
    
    /**
     * @brief 检查是否为单位变换
     * 
     * 由于四元数的双重覆盖性质，(1,0,0,0)和(-1,0,0,0)都表示单位旋转
     */
    bool isIdentity(double tolerance = constants::EPSILON) const {
        // 检查是否为 (1,0,0,0)
        bool is_positive_identity = std::abs(quat_.w() - 1.0) < tolerance &&
                                   std::abs(quat_.x()) < tolerance &&
                                   std::abs(quat_.y()) < tolerance &&
                                   std::abs(quat_.z()) < tolerance;
        
        // 检查是否为 (-1,0,0,0)
        bool is_negative_identity = std::abs(quat_.w() + 1.0) < tolerance &&
                                   std::abs(quat_.x()) < tolerance &&
                                   std::abs(quat_.y()) < tolerance &&
                                   std::abs(quat_.z()) < tolerance;
        
        return is_positive_identity || is_negative_identity;
    }
    
    /**
     * @brief 获取旋转角度（绕旋转轴的角度）
     */
    double getRotationAngle() const {
        return 2.0 * std::acos(std::abs(quat_.w()));
    }
    
    /**
     * @brief 获取旋转轴
     */
    Vector3 getRotationAxis() const {
        double sin_half_angle = std::sqrt(1.0 - quat_.w() * quat_.w());
        if (sin_half_angle < constants::EPSILON) {
            return Vector3(1.0, 0.0, 0.0);  // 任意轴，因为角度为0
        }
        return Vector3(quat_.x(), quat_.y(), quat_.z()) / sin_half_angle;
    }
    
    /**
     * @brief 球面线性插值
     */
    Transform slerp(const Transform& other, double t) const {
        return Transform(quat_.slerp(t, other.quat_));
    }
    
    /**
     * @brief 计算与另一个变换的角度差
     */
    double angleTo(const Transform& other) const {
        double dot = std::abs(quat_.dot(other.quat_));
        dot = std::min(1.0, dot);  // 防止数值误差
        return 2.0 * std::acos(dot);
    }
    
    /**
     * @brief 近似相等比较
     */
    bool isApprox(const Transform& other, double tolerance = constants::EPSILON) const {
        return angleTo(other) < tolerance;
    }
    
    /**
     * @brief 标准化四元数（确保单位长度）
     */
    void normalize() {
        quat_.normalize();
    }
    
private:
    /**
     * @brief 欧拉角转四元数的内部实现
     */
    static Quaternion eulerToQuaternion(double angle1, double angle2, double angle3, 
                                       EulerSequence sequence) {
        double c1 = std::cos(angle1 * 0.5);
        double s1 = std::sin(angle1 * 0.5);
        double c2 = std::cos(angle2 * 0.5);
        double s2 = std::sin(angle2 * 0.5);
        double c3 = std::cos(angle3 * 0.5);
        double s3 = std::sin(angle3 * 0.5);
        
        switch (sequence) {
            case EulerSequence::XYZ:
            /*
                数学表达式（XYZ=a1,a2,a3）:
                w = cos(a1/2) cos(a2/2) cos(a3/2) - sin(a1/2) sin(a2/2) sin(a3/2),
                x = sin(a1/2) cos(a2/2) cos(a3/2) + cos(a1/2) sin(a2/2) sin(a3/2), 
                y = cos(a1/2) sin(a2/2) cos(a3/2) - sin(a1/2) cos(a2/2) sin(a3/2), 
                z = cos(a1/2) cos(a2/2) sin(a3/2) + sin(a1/2) sin(a2/2) cos(a3/2)
            */
                return Quaternion(
                    c1*c2*c3 - s1*s2*s3,
                    s1*c2*c3 + c1*s2*s3,
                    c1*s2*c3 - s1*c2*s3,
                    c1*c2*s3 + s1*s2*c3
                );
            case EulerSequence::ZYX:
            /*
                数学表达式(ZYX=phi, psi, gamma)：
                w = cos(\phi/2) cos(\psi/2) cos(\gamma/2) + sin(\phi/2) sin(\psi/2) sin(\gamma/2);
                x = cos(\phi/2) cos(\psi/2) sin(\gamma/2) - sin(\phi/2) sin(\psi/2) cos(\gamma/2);
                y = cos(\phi/2) sin(\psi/2) cos(\gamma/2) + sin(\phi/2) cos(\psi/2) sin(\gamma/2);
                z = sin(\phi/2) cos(\psi/2) cos(\gamma/2) - cos(\phi/2) sin(\psi/2) sin(\gamma/2);

            */
                return Quaternion(
                    c1*c2*c3 + s1*s2*s3,
                    c1*c2*s3 - s1*s2*c3,
                    c1*s2*c3 + s1*c2*s3,
                    s1*c2*c3 - c1*s2*s3
                );
            // 可以根据需要添加更多序列
            default:
                return Quaternion(1.0, 0.0, 0.0, 0.0);
        }
    }
    
    /**
     * @brief 四元数转欧拉角的内部实现
     */
    static Vector3 quaternionToEuler(const Quaternion& q, EulerSequence sequence) {
        double w = q.w(), x = q.x(), y = q.y(), z = q.z();
        
        switch (sequence) {
            case EulerSequence::XYZ: {
                double angle1 = std::atan2(2*(w*x + y*z), 1 - 2*(x*x + y*y));
                double sin_angle2 = 2*(w*y - z*x);
                sin_angle2 = std::max(-1.0, std::min(1.0, sin_angle2));
                double angle2 = std::asin(sin_angle2);
                double angle3 = std::atan2(2*(w*z + x*y), 1 - 2*(y*y + z*z));
                return Vector3(angle1, angle2, angle3);
            }
            case EulerSequence::ZYX: {
                double angle1 = std::atan2(2*(w*z + x*y), 1 - 2*(y*y + z*z));
                double sin_angle2 = 2*(w*y - x*z);
                sin_angle2 = std::max(-1.0, std::min(1.0, sin_angle2));
                double angle2 = std::asin(sin_angle2);
                double angle3 = std::atan2(2*(w*x + y*z), 1 - 2*(x*x + y*y));
                return Vector3(angle1, angle2, angle3);
            }
            default:
                return Vector3(0.0, 0.0, 0.0);
        }
    }
};

// ==================== 便利函数 ====================

/**
 * @brief 球面线性插值的全局函数
 */
inline Transform Slerp(const Transform& from, const Transform& to, double t) {
    return from.slerp(to, t);
}

/**
 * @brief 计算两个变换之间角度的全局函数
 */
inline double AngleBetween(const Transform& t1, const Transform& t2) {
    return t1.angleTo(t2);
}

/**
 * @brief 近似相等比较的全局函数
 */
inline bool IsApprox(const Transform& t1, const Transform& t2, double tolerance = constants::EPSILON) {
    return t1.isApprox(t2, tolerance);
}

/**
 * @brief 便利函数命名空间
 * 
 * 提供一些常用的便利函数，简化常见操作
 */
namespace utils {

/**
 * @brief 创建绕X轴的旋转变换
 * @param angle 旋转角度（弧度）
 * @return 旋转变换
 */
inline Transform RotationX(double angle) {
    return Transform::RotationX(angle);
}

/**
 * @brief 创建绕Y轴的旋转变换
 * @param angle 旋转角度（弧度）
 * @return 旋转变换
 */
inline Transform RotationY(double angle) {
    return Transform::RotationY(angle);
}

/**
 * @brief 创建绕Z轴的旋转变换
 * @param angle 旋转角度（弧度）
 * @return 旋转变换
 */
inline Transform RotationZ(double angle) {
    return Transform::RotationZ(angle);
}

/**
 * @brief 创建绕任意轴的旋转变换
 * @param axis 旋转轴（单位向量）
 * @param angle 旋转角度（弧度）
 * @return 旋转变换
 */
inline Transform RotationAxis(const Vector3& axis, double angle) {
    return Transform::RotationAxis(axis, angle);
}

/**
 * @brief 球面线性插值（SLERP）
 * @param from 起始变换
 * @param to 目标变换
 * @param t 插值参数 [0, 1]
 * @return 插值后的变换
 */
inline Transform Slerp(const Transform& from, const Transform& to, double t) {
    return from.slerp(to, t);
}

/**
 * @brief 计算两个变换之间的角度差
 * @param from 起始变换
 * @param to 目标变换
 * @return 角度差（弧度）
 */
inline double AngleBetween(const Transform& from, const Transform& to) {
    return from.angleTo(to);
}

/**
 * @brief 检查两个变换是否近似相等
 * @param a 第一个变换
 * @param b 第二个变换
 * @param tolerance 容差（弧度）
 * @return 是否近似相等
 */
inline bool IsApprox(const Transform& a, const Transform& b, 
                     double tolerance = constants::EPSILON) {
    return a.isApprox(b, tolerance);
}

} // namespace utils

} // namespace transform
} // namespace math
} // namespace gnc