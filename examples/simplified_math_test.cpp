// 测试新的简化数学接口
#include "../../include/math/math.hpp"
#include "../../include/gnc/coordination/coordination.hpp"

// 现在可以直接使用这些类型，无需命名空间前缀！
void test_simplified_math() {
    // 直接使用Vector3d，无需命名空间
    Vector3d position(1.0, 2.0, 3.0);
    Vector4d homogeneous(1.0, 2.0, 3.0, 1.0);
    
    // 直接使用Transform，无需命名空间  
    Transform rotation = Transform::Identity();
    
    // 直接使用四元数，无需命名空间
    Quaterniond q = Quaterniond::Identity();
    
    // 直接使用数学常量，无需命名空间
    double angle = PI / 4.0;
    double degrees = angle * RAD_TO_DEG;
    
    // 使用欧拉角序列
    EulerSequence seq = EulerSequence::ZYX;
    
    // 坐标转换现在支持两种类型！
    
    // 1. 使用std::vector<double>（原有方式）
    std::vector<double> velocity_vec = {10.0, 0.0, 0.0};
    auto transformed_vec = SAFE_TRANSFORM_VEC(velocity_vec, "INERTIAL", "BODY");
    
    // 2. 使用Vector3d（新增方式，更直接）
    Vector3d velocity_eigen(10.0, 0.0, 0.0);
    auto transformed_eigen = SAFE_TRANSFORM_VEC(velocity_eigen, "INERTIAL", "BODY");
    
    // 两种方式都可以，编译器会自动选择正确的重载！
}