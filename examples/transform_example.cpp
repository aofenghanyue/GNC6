#include "math/transform/transform.hpp"
#include <iostream>
#include <iomanip>

using namespace gnc::math::transform;

int main() {
    std::cout << std::fixed << std::setprecision(6);
    
    std::cout << "=== 统一变换库使用示例 ===\n\n";
    
    // 1. 基本变换创建
    std::cout << "1. 基本变换创建:\n";
    
    // 创建单位变换
    auto identity = Transform::Identity();
    std::cout << "单位变换: " << identity.asQuaternion().coeffs().transpose() << "\n";
    
    // 从欧拉角创建
    auto euler_transform = Transform::FromEuler(0.1, 0.2, 0.3, EulerSequence::XYZ);
    std::cout << "欧拉角变换 (0.1, 0.2, 0.3): " 
              << euler_transform.asQuaternion().coeffs().transpose() << "\n";
    
    // 从矩阵创建
    Matrix3 rotation_matrix;
    rotation_matrix = Eigen::AngleAxisd(0.5, Vector3::UnitZ()).toRotationMatrix();
    auto matrix_transform = Transform::FromMatrix(rotation_matrix);
    std::cout << "矩阵变换 (绕Z轴0.5弧度): \n" 
              << matrix_transform.asMatrix() << "\n\n";
    
    // 从四元数创建
    auto quat_transform = Transform::FromQuaternion(0.707, 0.0, 0.0, 0.707);
    std::cout << "四元数变换: " << quat_transform.asQuaternion().coeffs().transpose() << "\n\n";
    
    // 2. 便捷的旋转创建
    std::cout << "2. 便捷的旋转创建:\n";
    
    auto rot_x = Transform::RotationX(0.5);
    auto rot_y = Transform::RotationY(0.5);
    auto rot_z = Transform::RotationZ(0.5);
    auto rot_axis = Transform::RotationAxis(Vector3(1, 1, 1).normalized(), 0.5);
    
    std::cout << "绕X轴旋转0.5弧度: " << rot_x.asEuler().transpose() << "\n";
    std::cout << "绕Y轴旋转0.5弧度: " << rot_y.asEuler().transpose() << "\n";
    std::cout << "绕Z轴旋转0.5弧度: " << rot_z.asEuler().transpose() << "\n";
    std::cout << "绕(1,1,1)轴旋转0.5弧度: " << rot_axis.asEuler().transpose() << "\n\n";
    
    // 3. 表示形式转换
    std::cout << "3. 表示形式转换:\n";
    
    auto test_transform = Transform::FromEuler(0.1, 0.2, 0.3, EulerSequence::XYZ);
    
    // 转换为不同表示形式
    auto as_quat = test_transform.asQuaternion();
    auto as_matrix = test_transform.asMatrix();
    auto as_euler_xyz = test_transform.asEuler(EulerSequence::XYZ);
    auto as_euler_zyx = test_transform.asEuler(EulerSequence::ZYX);
    
    std::cout << "四元数: " << as_quat.coeffs().transpose() << "\n";
    std::cout << "欧拉角(XYZ): " << as_euler_xyz.transpose() << "\n";
    std::cout << "欧拉角(ZYX): " << as_euler_zyx.transpose() << "\n";
    std::cout << "旋转矩阵: \n" << as_matrix << "\n\n";
    
    // 4. 向量变换
    std::cout << "4. 向量变换:\n";
    
    Vector3 test_vector(1.0, 0.0, 0.0);
    auto rotated_vector1 = test_transform.transform(test_vector);
    auto rotated_vector2 = test_transform * test_vector;  // 运算符重载
    
    std::cout << "原始向量: " << test_vector.transpose() << "\n";
    std::cout << "变换后向量(方法): " << rotated_vector1.transpose() << "\n";
    std::cout << "变换后向量(运算符): " << rotated_vector2.transpose() << "\n\n";
    
    // 5. 变换组合
    std::cout << "5. 变换组合:\n";
    
    auto transform1 = Transform::RotationX(0.1);
    auto transform2 = Transform::RotationY(0.2);
    auto combined1 = transform1.compose(transform2);
    auto combined2 = transform1 * transform2;  // 运算符重载
    
    std::cout << "变换1 (绕X轴0.1弧度): " << transform1.asEuler().transpose() << "\n";
    std::cout << "变换2 (绕Y轴0.2弧度): " << transform2.asEuler().transpose() << "\n";
    std::cout << "组合变换(方法): " << combined1.asEuler().transpose() << "\n";
    std::cout << "组合变换(运算符): " << combined2.asEuler().transpose() << "\n\n";
    
    // 6. 逆变换
    std::cout << "6. 逆变换:\n";
    
    auto original = Transform::FromEuler(0.1, 0.2, 0.3, EulerSequence::XYZ);
    auto inverse = original.inverse();
    auto should_be_identity = original * inverse;
    
    std::cout << "原始变换: " << original.asEuler().transpose() << "\n";
    std::cout << "逆变换: " << inverse.asEuler().transpose() << "\n";
    std::cout << "组合后 (应接近单位变换): " << should_be_identity.asEuler().transpose() << "\n";
    std::cout << "是否为单位变换: " << (should_be_identity.isIdentity() ? "是" : "否") << "\n\n";
    
    // 7. 球面线性插值 (SLERP)
    std::cout << "7. 球面线性插值 (SLERP):\n";
    
    auto start_transform = Transform::Identity();
    auto end_transform = Transform::RotationZ(1.0);
    
    for (double t = 0.0; t <= 1.0; t += 0.25) {
        auto interpolated1 = start_transform.slerp(end_transform, t);
        auto interpolated2 = Slerp(start_transform, end_transform, t);  // 全局函数
        std::cout << "t=" << t << ": " << interpolated1.asEuler().transpose() << "\n";
    }
    std::cout << "\n";
    
    // 8. 角度和轴分析
    std::cout << "8. 角度和轴分析:\n";
    
    auto test_rot = Transform::RotationAxis(Vector3(1, 1, 0).normalized(), 0.8);
    auto rotation_angle = test_rot.getRotationAngle();
    auto rotation_axis = test_rot.getRotationAxis();
    
    std::cout << "旋转角度: " << rotation_angle << " 弧度 (" 
              << rotation_angle * constants::RAD_TO_DEG << " 度)\n";
    std::cout << "旋转轴: " << rotation_axis.transpose() << "\n\n";
    
    // 9. 角度计算和比较
    std::cout << "9. 角度计算和比较:\n";
    
    auto angle_between1 = start_transform.angleTo(end_transform);
    auto angle_between2 = AngleBetween(start_transform, end_transform);  // 全局函数
    
    std::cout << "两个变换之间的角度: " << angle_between1 << " 弧度\n";
    std::cout << "转换为度数: " << angle_between1 * constants::RAD_TO_DEG << " 度\n";
    
    // 近似相等比较
    auto similar_transform = Transform::RotationZ(1.0001);
    std::cout << "近似相等比较: " << (end_transform.isApprox(similar_transform, 0.001) ? "是" : "否") << "\n\n";
    
    // 10. 链式操作演示
    std::cout << "10. 链式操作演示:\n";
    
    // 复杂的变换链
    auto complex_transform = Transform::RotationX(0.1)
                            * Transform::RotationY(0.2)
                            * Transform::RotationZ(0.3);
    
    std::cout << "链式变换结果: " << complex_transform.asEuler().transpose() << "\n";
    
    // 应用到向量
    Vector3 original_vector(1, 0, 0);
    Vector3 final_vector = complex_transform * original_vector;
    
    std::cout << "原始向量: " << original_vector.transpose() << "\n";
    std::cout << "最终向量: " << final_vector.transpose() << "\n\n";
    
    // 11. 不同欧拉角序列演示
    std::cout << "11. 不同欧拉角序列演示:\n";
    
    auto transform_xyz = Transform::FromEuler(0.1, 0.2, 0.3, EulerSequence::XYZ);
    auto euler_xyz = transform_xyz.asEuler(EulerSequence::XYZ);
    auto euler_zyx = transform_xyz.asEuler(EulerSequence::ZYX);
    
    std::cout << "同一变换的不同欧拉角表示:\n";
    std::cout << "XYZ序列: " << euler_xyz.transpose() << "\n";
    std::cout << "ZYX序列: " << euler_zyx.transpose() << "\n";
    
    // 验证它们表示同一个变换
    auto verify_xyz = Transform::FromEuler(euler_xyz, EulerSequence::XYZ);
    auto verify_zyx = Transform::FromEuler(euler_zyx, EulerSequence::ZYX);
    std::cout << "表示同一变换: " << (verify_xyz.isApprox(verify_zyx) ? "是" : "否") << "\n";
    
    return 0;
}