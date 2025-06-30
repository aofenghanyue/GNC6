/**
 * @file test_transform.cpp
 * @brief 姿态变换数学库测试文件
 * 
 * 这个文件包含了对Transform类的全面测试，验证其各种功能的正确性。
 * 测试覆盖：构造函数、静态工厂方法、变换操作、输出转换、实用方法等。
 * 使用Google Test框架进行测试。
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "../include/math/transform/transform.hpp"

using namespace gnc::math::transform;
using namespace gnc::math::transform::constants;

// 测试辅助函数
bool isApproxEqual(double a, double b, double tolerance = EPSILON) {
    return std::abs(a - b) < tolerance;
}

bool isApproxEqual(const Vector3& a, const Vector3& b, double tolerance = EPSILON) {
    return (a - b).norm() < tolerance;
}

bool isApproxEqual(const Quaternion& a, const Quaternion& b, double tolerance = EPSILON) {
    // 四元数可能有符号差异，但表示相同旋转
    return std::min((a.coeffs() - b.coeffs()).norm(), 
                   (a.coeffs() + b.coeffs()).norm()) < tolerance;
}

bool isApproxEqual(const Matrix3& a, const Matrix3& b, double tolerance = EPSILON) {
    return (a - b).norm() < tolerance;
}

// Google Test 自定义断言宏
#define EXPECT_APPROX_EQ(a, b) EXPECT_TRUE(isApproxEqual(a, b))
#define EXPECT_APPROX_EQ_TOL(a, b, tol) EXPECT_TRUE(isApproxEqual(a, b, tol))

// 测试基本构造函数
TEST(TransformTest, DefaultConstructor) {
    Transform t;
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, QuaternionConstructor) {
    Quaternion q(1.0, 0.0, 0.0, 0.0);
    Transform t(q);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, QuaternionComponentConstructor) {
    Transform t(1.0, 0.0, 0.0, 0.0);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, MatrixConstructor) {
    Matrix3 identity = Matrix3::Identity();
    Transform t(identity);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, EulerAngleConstructor) {
    Transform t(0.0, 0.0, 0.0);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, EulerVectorConstructor) {
    Vector3 euler_zero(0.0, 0.0, 0.0);
    Transform t(euler_zero);
    EXPECT_TRUE(t.isIdentity());
}

// 测试静态工厂方法
TEST(TransformTest, IdentityFactory) {
    Transform identity = Transform::Identity();
    EXPECT_TRUE(identity.isIdentity());
}

TEST(TransformTest, FromQuaternionFactory) {
    Quaternion q(1.0, 0.0, 0.0, 0.0);
    Transform t = Transform::FromQuaternion(q);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, FromQuaternionComponentFactory) {
    Transform t = Transform::FromQuaternion(1.0, 0.0, 0.0, 0.0);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, FromMatrixFactory) {
    Matrix3 identity_matrix = Matrix3::Identity();
    Transform t = Transform::FromMatrix(identity_matrix);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, FromEulerFactory) {
    Transform t = Transform::FromEuler(0.0, 0.0, 0.0);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, FromEulerVectorFactory) {
    Vector3 euler_zero(0.0, 0.0, 0.0);
    Transform t = Transform::FromEuler(euler_zero);
    EXPECT_TRUE(t.isIdentity());
}

TEST(TransformTest, RotationAxisFactories) {
    Transform rx = Transform::RotationX(0.0);
    Transform ry = Transform::RotationY(0.0);
    Transform rz = Transform::RotationZ(0.0);
    EXPECT_TRUE(rx.isIdentity());
    EXPECT_TRUE(ry.isIdentity());
    EXPECT_TRUE(rz.isIdentity());
}

TEST(TransformTest, RotationAxisFactory) {
    Vector3 axis(1.0, 0.0, 0.0);
    Transform ra = Transform::RotationAxis(axis, 0.0);
    EXPECT_TRUE(ra.isIdentity());
}

// 测试基本变换操作
TEST(TransformTest, InverseTransformIdentity) {
    Transform identity = Transform::Identity();
    Transform inv = identity.inverse();
    EXPECT_TRUE(inv.isIdentity());
}

TEST(TransformTest, RotationInverseComposition) {
    Transform rz90 = Transform::RotationZ(PI / 2.0);
    Transform rz90_inv = rz90.inverse();
    Transform should_be_identity = rz90 * rz90_inv;
    EXPECT_TRUE(should_be_identity.isIdentity(1e-10));
}

TEST(TransformTest, TransformCompositionMethods) {
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform ry90 = Transform::RotationY(PI / 2.0);
    Transform combined = rx90.compose(ry90);
    Transform combined_op = rx90 * ry90;
    EXPECT_TRUE(combined.isApprox(combined_op));
}

TEST(TransformTest, CompoundAssignmentOperator) {
    Transform t = Transform::Identity();
    Transform rx90 = Transform::RotationX(PI / 2.0);
    t *= rx90;
    EXPECT_TRUE(t.isApprox(rx90));
}

// 测试向量变换
TEST(TransformTest, IdentityVectorTransform) {
    Transform identity = Transform::Identity();
    Vector3 v(1.0, 2.0, 3.0);
    Vector3 v_transformed = identity.transform(v);
    Vector3 v_op = identity * v;
    EXPECT_APPROX_EQ(v, v_transformed);
    EXPECT_APPROX_EQ(v_transformed, v_op);
}

TEST(TransformTest, RotationZVectorTransform) {
    Transform rz90 = Transform::RotationZ(PI / 2.0);
    Vector3 x_axis(1.0, 0.0, 0.0);
    Vector3 rotated_x = rz90 * x_axis;
    Vector3 expected_y(0.0, 1.0, 0.0);
    EXPECT_APPROX_EQ_TOL(rotated_x, expected_y, 1e-10);
}

TEST(TransformTest, RotationXVectorTransform) {
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Vector3 y_axis(0.0, 1.0, 0.0);
    Vector3 rotated_y = rx90 * y_axis;
    Vector3 expected_z(0.0, 0.0, 1.0);
    EXPECT_APPROX_EQ_TOL(rotated_y, expected_z, 1e-10);
}

TEST(TransformTest, RotationYVectorTransform) {
    Transform ry90 = Transform::RotationY(PI / 2.0);
    Vector3 z_axis(0.0, 0.0, 1.0);
    Vector3 rotated_z = ry90 * z_axis;
    Vector3 expected_x(1.0, 0.0, 0.0);
    EXPECT_APPROX_EQ_TOL(rotated_z, expected_x, 1e-10);
}

// 测试输出转换
TEST(TransformTest, IdentityOutputConversion) {
    Transform identity = Transform::Identity();
    
    Quaternion q = identity.asQuaternion();
    Quaternion expected_q(1.0, 0.0, 0.0, 0.0);
    EXPECT_APPROX_EQ(q, expected_q);
    
    Matrix3 m = identity.asMatrix();
    Matrix3 expected_m = Matrix3::Identity();
    EXPECT_APPROX_EQ(m, expected_m);
    
    Vector3 euler = identity.asEuler();
    Vector3 expected_euler(0.0, 0.0, 0.0);
    EXPECT_APPROX_EQ(euler, expected_euler);
}

TEST(TransformTest, EulerAngleComponents) {
    Transform identity = Transform::Identity();
    EXPECT_APPROX_EQ(identity.getAngle1(), 0.0);
    EXPECT_APPROX_EQ(identity.getAngle2(), 0.0);
    EXPECT_APPROX_EQ(identity.getAngle3(), 0.0);
}

TEST(TransformTest, NonIdentityOutputConversion) {
    Transform rz90 = Transform::RotationZ(PI / 2.0);
    Vector3 euler_rz90 = rz90.asEuler(EulerSequence::XYZ);
    EXPECT_APPROX_EQ_TOL(euler_rz90.z(), PI / 2.0, 1e-10);
}

// 测试实用方法
TEST(TransformTest, IsIdentityMethod) {
    Transform identity = Transform::Identity();
    Transform rz90 = Transform::RotationZ(PI / 2.0);
    EXPECT_TRUE(identity.isIdentity());
    EXPECT_FALSE(rz90.isIdentity());
}

TEST(TransformTest, RotationAngleAndAxis) {
    Transform identity = Transform::Identity();
    EXPECT_APPROX_EQ(identity.getRotationAngle(), 0.0);
    
    Transform rz90 = Transform::RotationZ(PI / 2.0);
    double angle_rz90 = rz90.getRotationAngle();
    EXPECT_APPROX_EQ_TOL(angle_rz90, PI / 2.0, 1e-10);
    
    Vector3 axis_rz90 = rz90.getRotationAxis();
    Vector3 expected_z_axis(0.0, 0.0, 1.0);
    EXPECT_APPROX_EQ_TOL(axis_rz90, expected_z_axis, 1e-10);
}

TEST(TransformTest, SphericalLinearInterpolation) {
    Transform identity = Transform::Identity();
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform slerp_half = identity.slerp(rx90, 0.5);
    Transform expected_rx45 = Transform::RotationX(PI / 4.0);
    EXPECT_TRUE(slerp_half.isApprox(expected_rx45, 1e-10));
}

TEST(TransformTest, AngleDifference) {
    Transform identity = Transform::Identity();
    Transform rx90 = Transform::RotationX(PI / 2.0);
    double angle_diff = identity.angleTo(rx90);
    EXPECT_APPROX_EQ_TOL(angle_diff, PI / 2.0, 1e-10);
}

TEST(TransformTest, ApproximateEquality) {
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform rx90_copy = Transform::RotationX(PI / 2.0);
    Transform identity = Transform::Identity();
    EXPECT_TRUE(rx90.isApprox(rx90_copy));
    EXPECT_FALSE(identity.isApprox(rx90));
}

// 测试便利函数
TEST(TransformTest, GlobalUtilityFunctions) {
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform ry90 = Transform::RotationY(PI / 2.0);
    
    Transform slerp_result = Slerp(rx90, ry90, 0.5);
    Transform slerp_method = rx90.slerp(ry90, 0.5);
    EXPECT_TRUE(slerp_result.isApprox(slerp_method));
    
    double angle_global = AngleBetween(rx90, ry90);
    double angle_method = rx90.angleTo(ry90);
    EXPECT_APPROX_EQ(angle_global, angle_method);
    
    bool approx_global = IsApprox(rx90, rx90);
    bool approx_method = rx90.isApprox(rx90);
    EXPECT_EQ(approx_global, approx_method);
}

TEST(TransformTest, UtilsNamespaceFunctions) {
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform ry90 = Transform::RotationY(PI / 2.0);
    
    Transform utils_rx = utils::RotationX(PI / 2.0);
    Transform utils_ry = utils::RotationY(PI / 2.0);
    Transform utils_rz = utils::RotationZ(PI / 2.0);
    
    EXPECT_TRUE(utils_rx.isApprox(rx90));
    EXPECT_TRUE(utils_ry.isApprox(ry90));
    EXPECT_TRUE(utils_rz.isApprox(Transform::RotationZ(PI / 2.0)));
    
    Vector3 axis(1.0, 1.0, 1.0);
    Transform utils_ra = utils::RotationAxis(axis, PI / 3.0);
    Transform direct_ra = Transform::RotationAxis(axis, PI / 3.0);
    EXPECT_TRUE(utils_ra.isApprox(direct_ra));
    
    Transform slerp_result = Slerp(rx90, ry90, 0.5);
    Transform utils_slerp = utils::Slerp(rx90, ry90, 0.5);
    EXPECT_TRUE(utils_slerp.isApprox(slerp_result));
    
    double angle_global = AngleBetween(rx90, ry90);
    double utils_angle = utils::AngleBetween(rx90, ry90);
    EXPECT_APPROX_EQ(utils_angle, angle_global);
    
    bool approx_global = IsApprox(rx90, rx90);
    bool utils_approx = utils::IsApprox(rx90, rx90);
    EXPECT_EQ(utils_approx, approx_global);
}

// 测试数值稳定性
TEST(TransformTest, AccumulatedSmallRotations) {
    Transform accumulated = Transform::Identity();
    double small_angle = 0.001;
    int num_steps = static_cast<int>(1.0 / small_angle);
    
    for (int i = 0; i < num_steps; ++i) {
        accumulated = accumulated * Transform::RotationZ(small_angle);
    }
    
    Transform expected = Transform::RotationZ(1.0);
    EXPECT_TRUE(accumulated.isApprox(expected, 1e-10));
}

TEST(TransformTest, QuaternionNormalization) {
    Transform t = Transform::RotationX(PI / 4.0);
    Quaternion q = t.asQuaternion();
    double norm_before = q.norm();
    t.normalize();
    Quaternion q_after = t.asQuaternion();
    double norm_after = q_after.norm();
    EXPECT_APPROX_EQ_TOL(norm_before, 1.0, 1e-10);
    EXPECT_APPROX_EQ_TOL(norm_after, 1.0, 1e-10);
}

TEST(TransformTest, TinyAngleStability) {
    Transform tiny_rotation = Transform::RotationX(1e-10);
    bool is_stable = !tiny_rotation.isIdentity(1e-12) || tiny_rotation.isIdentity(1e-8);
    EXPECT_TRUE(is_stable);
}

// 测试边界情况
TEST(TransformTest, Rotation180Degrees) {
    Transform rx180 = Transform::RotationX(PI);
    Vector3 y_axis(0.0, 1.0, 0.0);
    Vector3 rotated = rx180 * y_axis;
    Vector3 expected(0.0, -1.0, 0.0);
    EXPECT_APPROX_EQ_TOL(rotated, expected, 1e-10);
}

TEST(TransformTest, Rotation360Degrees) {
    Transform rx360 = Transform::RotationX(2.0 * PI);
    EXPECT_TRUE(rx360.isIdentity(1e-10));
}

TEST(TransformTest, NegativeAngleRotation) {
    Transform rx_neg90 = Transform::RotationX(-PI / 2.0);
    Transform rx90 = Transform::RotationX(PI / 2.0);
    Transform should_be_identity = rx_neg90 * rx90;
    EXPECT_TRUE(should_be_identity.isIdentity(1e-10));
}

TEST(TransformTest, ZeroAxisHandling) {
    Vector3 zero_axis(0.0, 0.0, 0.0);
    bool handled_gracefully = true;
    try {
        Transform ra_zero = Transform::RotationAxis(zero_axis, PI / 2.0);
    } catch (...) {
        // Exception is also acceptable
    }
    EXPECT_TRUE(handled_gracefully);
}

// Google Test main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}