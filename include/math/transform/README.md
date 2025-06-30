# 统一变换库

这是一个专门用于3D旋转变换的数学库，从GNC框架的坐标系统模块中抽离出来，提供纯数学的旋转变换功能。采用统一接口设计，内部使用四元数进行高效计算。

## 设计理念

**统一接口，内部优化**：用户只需要使用一个 `Transform` 类，无需关心内部表示形式。库内部统一使用四元数进行计算以获得最佳性能和数值稳定性，但可以接受和输出任何表示形式。

## 功能特性

- **统一接口**: 只需使用一个 `Transform` 类处理所有旋转变换
- **高性能**: 内部统一使用四元数计算，避免不必要的转换
- **灵活输入**: 支持从四元数、旋转矩阵、欧拉角等多种形式构造
- **灵活输出**: 可以输出为四元数、旋转矩阵、欧拉角等任意形式
- **链式操作**: 支持变换组合和运算符重载
- **数值稳定**: 内置数值稳定性保证和自动归一化
- **便利函数**: 提供常用的旋转操作和工具函数

## 文件结构

```
include/math/transform/
├── types.hpp                 # 基础类型定义和常量
├── attitude_transform.hpp    # 抽象接口定义
├── quaternion_transform.hpp  # 四元数变换实现
├── matrix_transform.hpp      # 矩阵变换实现
├── euler_transform.hpp       # 欧拉角变换实现
├── transform_impl.hpp        # 实现细节和相互转换
├── transform.hpp             # 主头文件（包含所有功能）
└── README.md                 # 本文档
```

## 快速开始

### 基本使用

```cpp
#include "math/transform/transform.hpp"
using namespace math::transform;

// 创建变换 - 支持多种输入方式
auto transform1 = Transform::FromEuler(0.1, 0.2, 0.3);           // 从欧拉角
auto transform2 = Transform::FromQuaternion(1, 0, 0, 0);         // 从四元数
auto transform3 = Transform::RotationZ(0.5);                     // 绕Z轴旋转
auto transform4 = Transform::RotationAxis(Vector3(1,1,1), 0.8);  // 绕任意轴旋转

// 变换向量 - 支持运算符重载
Vector3 vector(1.0, 0.0, 0.0);
Vector3 rotated1 = transform1.transform(vector);  // 方法调用
Vector3 rotated2 = transform1 * vector;           // 运算符重载

// 组合变换 - 支持链式操作
auto combined1 = transform1.compose(transform2);   // 方法调用
auto combined2 = transform1 * transform2;          // 运算符重载
auto complex = Transform::RotationX(0.1) * Transform::RotationY(0.2) * Transform::RotationZ(0.3);

// 输出为任意表示形式
auto quat = transform1.asQuaternion();             // 四元数
auto matrix = transform1.asMatrix();               // 旋转矩阵
auto euler_xyz = transform1.asEuler(EulerSequence::XYZ);  // XYZ欧拉角
auto euler_zyx = transform1.asEuler(EulerSequence::ZYX);  // ZYX欧拉角

// 实用功能
auto inverse = transform1.inverse();               // 逆变换
auto interpolated = transform1.slerp(transform2, 0.5);  // 球面插值
double angle = transform1.angleTo(transform2);     // 角度差
bool similar = transform1.isApprox(transform2);    // 近似相等
```

### 便利函数

```cpp
using namespace gnc::math::transform::utils;

// 创建单轴旋转
auto rotX = RotationX(M_PI/4);  // 绕X轴旋转45度
auto rotY = RotationY(M_PI/3);  // 绕Y轴旋转60度
auto rotZ = RotationZ(M_PI/6);  // 绕Z轴旋转30度

// 绕任意轴旋转
Vector3 axis(1, 1, 0);  // 旋转轴
double angle = M_PI/4;  // 旋转角度
auto rotAxis = RotationAxis(axis, angle);

// 球面线性插值
auto interpolated = Slerp(*rotX, *rotY, 0.5);

// 角度计算
double angleDiff = AngleBetween(*rotX, *rotY);
```

## 核心概念

### 1. 统一接口设计

`Transform` 类提供了一个完全统一的接口来处理所有类型的旋转变换：

```cpp
class Transform {
public:
    // 多种构造方式
    Transform();  // 单位变换
    Transform(const Quaternion& q);  // 从四元数
    Transform(const Matrix3& matrix);  // 从矩阵
    Transform(double angle1, double angle2, double angle3, EulerSequence sequence);  // 从欧拉角
    
    // 静态工厂方法
    static Transform Identity();
    static Transform FromQuaternion(const Quaternion& q);
    static Transform FromMatrix(const Matrix3& matrix);
    static Transform FromEuler(double a1, double a2, double a3, EulerSequence seq);
    static Transform RotationX/Y/Z(double angle);
    static Transform RotationAxis(const Vector3& axis, double angle);
    
    // 变换操作
    Transform inverse() const;
    Transform compose(const Transform& other) const;
    Vector3 transform(const Vector3& vector) const;
    
    // 运算符重载
    Transform operator*(const Transform& other) const;  // 变换组合
    Vector3 operator*(const Vector3& vector) const;     // 向量变换
    
    // 输出转换
    Quaternion asQuaternion() const;
    Matrix3 asMatrix() const;
    Vector3 asEuler(EulerSequence sequence) const;
    
    // 实用方法
    Transform slerp(const Transform& other, double t) const;
    double angleTo(const Transform& other) const;
    bool isApprox(const Transform& other, double tolerance) const;
};
```

### 2. 内部实现优势

新的统一设计采用四元数作为内部表示，具有以下优势：

| 方面 | 统一Transform类 | 传统多类设计 |
|------|----------------|-------------|
| **易用性** | 单一类，无需选择 | 需要选择合适的表示类 |
| **性能** | 内部四元数，高效计算 | 不同类间转换开销 |
| **内存** | 固定4个浮点数 | 根据选择变化(3-9个) |
| **数值稳定** | 自动归一化保证 | 需要手动维护 |
| **组合操作** | 原生四元数乘法 | 可能需要转换 |
| **插值** | 原生SLERP支持 | 需要转换到四元数 |
| **奇异点** | 无万向节锁问题 | 欧拉角有万向节锁 |

**设计优势：**
- **零学习成本**: 用户无需了解不同表示形式的特点
- **最优性能**: 内部始终使用最适合计算的四元数
- **灵活输入输出**: 支持任意表示形式的输入和输出
- **自动优化**: 避免不必要的表示转换

### 3. 欧拉角序列

支持多种欧拉角序列：
- `ZYX`: Z-Y-X序列（最常用，航空航天标准）
- `XYZ`: X-Y-Z序列
- `YZX`, `ZXY`, `XZY`, `YXZ`: 其他序列

## 设计原则

### 1. 单一职责原则
每个类只负责一种表示形式的变换，职责明确。

### 2. 开闭原则
通过抽象接口设计，可以轻松扩展新的表示形式。

### 3. 里氏替换原则
所有实现类都可以通过基类接口使用，保证多态性。

### 4. 接口隔离原则
接口设计精简，只包含必要的方法。

### 5. 依赖倒置原则
高层模块依赖抽象接口，而非具体实现。

## 数值稳定性

### 四元数
- 自动归一化，确保单位四元数性质
- 使用Shepperd方法进行矩阵到四元数的转换

### 旋转矩阵
- 验证正交性和行列式
- 提供Gram-Schmidt正交化功能
- 自动修正数值误差

### 欧拉角
- 检测万向节锁情况
- 提供角度标准化功能
- 通过四元数中转避免累积误差

## 性能考虑

### 内存使用
- 四元数：4个double (32字节)
- 旋转矩阵：9个double (72字节)
- 欧拉角：3个double + 枚举 (≈32字节)

### 计算性能
- 向量变换：矩阵 > 四元数 > 欧拉角
- 变换组合：四元数 > 矩阵 > 欧拉角
- 插值运算：四元数最优（支持SLERP）

## 最佳实践

### 1. 统一使用Transform类

```cpp
// 推荐：统一使用Transform类
auto transform1 = Transform::FromEuler(0.1, 0.2, 0.3);
auto transform2 = Transform::RotationZ(0.5);
auto combined = transform1 * transform2;  // 高效的四元数运算

// 不推荐：混用不同的表示类（旧设计）
// auto q1 = TransformQuaternion::FromEuler(...);
// auto m1 = TransformMatrix::FromQuaternion(q1);  // 不必要的转换
```

### 2. 利用运算符重载

```cpp
// 链式变换组合
auto complex_transform = Transform::RotationX(0.1)
                       * Transform::RotationY(0.2) 
                       * Transform::RotationZ(0.3);

// 批量向量变换
std::vector<Vector3> transformed_points;
for (const auto& point : original_points) {
    transformed_points.push_back(transform * point);
}
```

### 3. 按需输出转换

```cpp
// 只在需要时转换输出格式
auto transform = Transform::FromEuler(0.1, 0.2, 0.3);

// 用于显示
auto euler_for_display = transform.asEuler(EulerSequence::ZYX);
std::cout << "Angles: " << euler_for_display.transpose() << std::endl;

// 用于图形API
auto matrix_for_opengl = transform.asMatrix();
glMultMatrixd(matrix_for_opengl.data());

// 用于序列化
auto quat_for_save = transform.asQuaternion();
save_quaternion(quat_for_save);
```

### 4. 数值稳定性自动保证

```cpp
// Transform类自动处理数值稳定性
auto transform = Transform::FromQuaternion(w, x, y, z);  // 自动归一化

// 长时间累积后仍然稳定
for (int i = 0; i < 10000; ++i) {
    transform *= small_rotation;  // 内部自动维护数值稳定性
}

// 使用内置的近似比较
if (transform1.isApprox(transform2, 1e-6)) {
    // 认为相等
}
```

### 5. 性能优化建议

```cpp
// 预计算常用变换
static const auto ROTATION_90_Z = Transform::RotationZ(M_PI/2);
static const auto ROTATION_180_X = Transform::RotationX(M_PI);

// 使用球面插值进行平滑过渡
auto interpolated = start_transform.slerp(end_transform, t);

// 批量操作时复用变换
const auto& transform_ref = get_transform();  // 避免拷贝
for (const auto& point : points) {
    result.push_back(transform_ref * point);
}
```

## 扩展指南

如需添加新的表示形式（如轴角表示），请：

1. 继承`IAttitudeTransform`接口
2. 实现所有纯虚函数
3. 在`transform_impl.hpp`中添加转换方法
4. 在`transform.hpp`中添加工厂方法
5. 更新文档和示例

## 注意事项

1. **线程安全**: 所有变换对象都是不可变的，天然线程安全
2. **数值精度**: 使用double精度，适合大多数应用场景
3. **内存管理**: 使用智能指针，自动管理内存
4. **异常安全**: 输入验证和异常处理完善

## 参考资料

- [Quaternions and Rotation Sequences](https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation)
- [Euler Angles](https://en.wikipedia.org/wiki/Euler_angles)
- [Rotation Matrix](https://en.wikipedia.org/wiki/Rotation_matrix)
- [Gimbal Lock](https://en.wikipedia.org/wiki/Gimbal_lock)