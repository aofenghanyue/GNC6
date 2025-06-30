# 坐标系统模块设计文档（修订版）

## 1. 模块概述

本模块提供无人机仿真中坐标系及其转换关系的管理功能。它采用图论建模坐标系统间的变换关系，通过自动路径查找实现任意坐标系间的转换推导。该模块将坐标变换抽象为群的概念，使得不同的表示方式（四元数、矩阵、欧拉角）只是同一变换的不同表达方式。

### 1.1 主要特性

- **图论建模**：使用有向图表示坐标系关系，节点为坐标系，边为变换关系
- **自动推导**：支持自动查找和组合多步变换路径
- **统一变换接口**：不同表示形式使用同一抽象接口，实现坐标变换群的一致性表达
- **静态与动态变换**：同时支持固定变换关系和基于状态的动态变换
- **高效缓存机制**：双层版本控制优化路径查找和变换计算
- **无锁读取**：读操作无需锁定，提高并发性能
- **数值稳定性**：自动处理四元数归一化和矩阵正交化等数值问题

### 1.2 应用场景

- **多机编队**：管理多个飞行器的机体坐标系
- **传感器融合**：处理不同传感器坐标系的转换
- **轨迹规划**：在不同参考系下进行路径计算
- **视觉导航**：处理相机坐标系与世界坐标系的关系

## 2. 核心概念

本章深入探讨坐标变换系统的核心概念和设计理念，旨在帮助新手开发者理解系统架构和实现细节。

### 2.1 坐标系统标识符(CoordinateSystemIdentifier)

#### 2.1.1 设计理念

坐标系统标识符是整个系统的基础，它解决了一个关键问题：**如何唯一标识并区分不同的坐标系统**。在设计这个标识符时，我们需要考虑两个维度：

1. **坐标系类型维度**：区分NED、ENU、BODY等不同类型的坐标系
2. **实例维度**：区分同一类型但属于不同飞行器的坐标系

例如，两个飞行器各自有自己的机体坐标系(BODY)，它们在类型上相同，但在实例上不同。

#### 2.1.2 实现详解

```cpp
struct CoordinateSystemIdentifier {
    CoordinateSystemType type{};                // 基础坐标类型
    std::optional<uint32_t> vehicleId{};        // 关联的飞行器ID（空表示通用）

    // 默认构造函数
    CoordinateSystemIdentifier() = default;

    // 从基础类型构造
    explicit CoordinateSystemIdentifier(CoordinateSystemType t) 
        : type(t), vehicleId(std::nullopt) {}

    // 完整构造函数
    CoordinateSystemIdentifier(CoordinateSystemType t, std::optional<uint32_t> id) 
        : type(t), vehicleId(id) {}

    // 隐式转换运算符保持原有接口兼容
    operator CoordinateSystemType() const { return type; }

    // 相等运算符
    bool operator==(const CoordinateSystemIdentifier& other) const {
        return type == other.type && vehicleId == other.vehicleId;
    }
};
```

#### 2.1.3 实现亮点解析

1. **使用`std::optional<uint32_t>`表示飞行器ID**：
   - 当值为`std::nullopt`时，表示通用坐标系（如全局NED系）
   - 当有具体值时，表示特定飞行器的坐标系（如飞行器1的机体系）
   
   ```cpp
   // 创建全局NED坐标系标识符
   auto globalNed = CoordinateSystemIdentifier(CoordinateSystemType::NED);
   
   // 创建飞行器1的机体坐标系标识符
   auto uav1Body = CoordinateSystemIdentifier(CoordinateSystemType::BODY, 1);
   ```

2. **隐式转换运算符**：
   ```cpp
   operator CoordinateSystemType() const { return type; }
   ```
   这允许将标识符直接用在需要坐标系类型的上下文中，保持向后兼容：
   
   ```cpp
   void processCoordinateType(CoordinateSystemType type);
   
   CoordinateSystemIdentifier id{CoordinateSystemType::NED};
   processCoordinateType(id);  // 隐式转换为CoordinateSystemType::NED
   ```

3. **自定义相等运算符**：确保在比较和查找时，同时考虑类型和ID两个维度

4. **为哈希表自定义哈希函数**：在`std::namespace`中实现自定义哈希函数，支持在`std::unordered_map`等容器中使用，如代码中所示：

   ```cpp
   namespace std {
   template<>
   struct hash<gnc::CoordinateSystemIdentifier> {
       size_t operator()(const gnc::CoordinateSystemIdentifier& id) const {
           // 组合类型和ID的哈希值
           return std::hash<uint32_t>{}(static_cast<uint32_t>(id.type)) ^ 
                  std::hash<uint32_t>{}(id.vehicleId.value_or(0));
       }
   };
   } // namespace std
   ```

#### 2.1.4 使用示例与最佳实践

```cpp
// 示例1：创建和比较坐标系标识符
auto nedId = CoordinateSystemIdentifier(CoordinateSystemType::NED);
auto enuId = CoordinateSystemIdentifier(CoordinateSystemType::ENU);
auto uav1BodyId = CoordinateSystemIdentifier(CoordinateSystemType::BODY, 1);
auto uav2BodyId = CoordinateSystemIdentifier(CoordinateSystemType::BODY, 2);

// 比较是否为同一坐标系
bool isSameSystem = (uav1BodyId == uav2BodyId);  // false，不同飞行器

// 示例2：在哈希表中使用
std::unordered_map<CoordinateSystemIdentifier, std::string> coordinateNames;
coordinateNames[nedId] = "全局NED坐标系";
coordinateNames[uav1BodyId] = "飞行器1机体坐标系";

// 示例3：使用命名空间管理常用坐标系标识符
namespace CoordinateSystems {
    const CoordinateSystemIdentifier NED{CoordinateSystemType::NED};
    const CoordinateSystemIdentifier ENU{CoordinateSystemType::ENU};
    const CoordinateSystemIdentifier ECEF{CoordinateSystemType::ECEF};
    
    inline CoordinateSystemIdentifier BODY(uint32_t vehicleId) {
        return {CoordinateSystemType::BODY, vehicleId};
    }
}

// 使用命名空间中的标识符
auto transform = manager.getTransform(
    CoordinateSystems::BODY(1), 
    CoordinateSystems::NED
);
```

#### 2.1.5 编程提示

- **使用常量而非硬编码**：避免直接使用`CoordinateSystemType::XXX`，而是使用定义好的常量
- **设置合理的默认值**：使用`std::nullopt`表示通用坐标系，避免使用特殊数值（如-1）
- **遵循最小权限原则**：只有必要时才提供全局飞行器ID

### 2.2 姿态变换接口(IAttitudeTransform)

#### 2.2.1 设计理念

姿态变换接口是整个坐标变换系统的核心，它抽象了不同表示方法（四元数、矩阵、欧拉角）的共同行为。这种设计基于以下关键洞见：

- **同构性**：旋转变换形成一个数学群，无论用什么方法表示，它们描述的是相同的数学实体
- **多样表示**：不同场景下不同表示形式各有优缺点
- **统一接口**：通过抽象接口屏蔽表示细节，简化系统设计

#### 2.2.2 实现详解

根据`attitude_transform.hpp`中的定义，`IAttitudeTransform`接口如下：

```cpp
class IAttitudeTransform {
public:
    virtual ~IAttitudeTransform() = default;

    // 基本变换操作
    virtual std::shared_ptr<IAttitudeTransform> inverse() const = 0;
    virtual std::shared_ptr<IAttitudeTransform> compose(const IAttitudeTransform& other) const = 0;
    virtual Vector3 transform(const Vector3& v) const = 0;

    // 表示形式转换
    virtual TransformQuaternion asQuaternion() const = 0;
    virtual TransformMatrix asMatrix() const = 0;
    virtual EulerAngles asEuler(EulerSequence sequence = EulerSequence::ZYX) const = 0;
    virtual Vector4 asAxisAngle() const = 0;

    // 静态工厂方法
    static std::shared_ptr<IAttitudeTransform> Identity();
    static std::shared_ptr<IAttitudeTransform> FromQuaternion(const Quaternion& q);
    static std::shared_ptr<IAttitudeTransform> FromMatrix(const Matrix3& m);
    static std::shared_ptr<IAttitudeTransform> FromEuler(const Vector3& euler, 
        EulerSequence sequence = EulerSequence::ZYX);
    static std::shared_ptr<IAttitudeTransform> FromAxisAngle(const Vector4& axisAngle);

protected:
    // 保护构造函数，供派生类使用
    IAttitudeTransform() = default;
    IAttitudeTransform(const IAttitudeTransform&) = default;
    IAttitudeTransform(const Quaternion&) {}
    IAttitudeTransform(const Matrix3&) {}
    IAttitudeTransform(const Vector3&, EulerSequence = EulerSequence::ZYX) {}
    IAttitudeTransform(const Vector4&) {}
};
```

#### 2.2.3 接口方法解析

1. **基本变换操作**：
   - `inverse()`: 计算逆变换，对旋转变换相当于转置
   - `compose()`: 将两个变换组合，数学上相当于矩阵乘法
   - `transform()`: 对向量应用变换，实现坐标系转换

2. **表示形式转换**：实现不同表示间的互转
   - `asQuaternion()`: 转为四元数表示
   - `asMatrix()`: 转为矩阵表示
   - `asEuler()`: 转为欧拉角表示，默认ZYX顺序（航空航天常用）
   - `asAxisAngle()`: 转为轴角表示（旋转轴+旋转角）

3. **静态工厂方法**：提供多种创建变换的方式
   - `Identity()`: 创建单位变换（不改变向量）
   - `FromXXX()`: 从不同表示形式创建变换

#### 2.2.4 设计模式解析

该接口应用了多种设计模式：

1. **策略模式**：不同实现类提供相同操作的不同算法（四元数乘法、矩阵乘法等）

2. **工厂方法模式**：通过静态工厂方法创建具体实现类
   ```cpp
   // 使用工厂方法创建变换
   auto transform = IAttitudeTransform::FromEuler(
       Vector3(0.1, 0.2, 0.3), EulerSequence::ZYX);
   ```

3. **桥接模式**：将抽象与实现分离，允许它们独立变化

4. **适配器模式**：允许不同表示方法之间的转换
   ```cpp
   // 从四元数创建，但以矩阵形式使用
   auto transform = IAttitudeTransform::FromQuaternion(quat);
   Matrix3 rotationMatrix = transform->asMatrix();
   ```

#### 2.2.5 使用示例与最佳实践

```cpp
// 示例1：创建变换并应用于向量
auto transform = IAttitudeTransform::FromEuler(
    Vector3(M_PI/6, 0, 0), EulerSequence::ZYX);  // 绕X轴旋转30度

Vector3 originalPoint(1, 0, 0);
Vector3 transformedPoint = transform->transform(originalPoint);

// 示例2：组合多个变换
auto transform1 = IAttitudeTransform::FromEuler(
    Vector3(M_PI/6, 0, 0), EulerSequence::ZYX);  // 绕X轴旋转30度
auto transform2 = IAttitudeTransform::FromEuler(
    Vector3(0, M_PI/4, 0), EulerSequence::ZYX);  // 绕Y轴旋转45度

// 组合：先旋转transform2，再旋转transform1
auto combinedTransform = transform1->compose(*transform2);
Vector3 result = combinedTransform->transform(originalPoint);

// 示例3：计算逆变换
auto inverseTransform = transform->inverse();
Vector3 recoveredPoint = inverseTransform->transform(transformedPoint);
// recoveredPoint应该接近originalPoint

// 示例4：不同表示形式间转换
auto quatTransform = IAttitudeTransform::FromQuaternion(someQuaternion);
// 获取等效的欧拉角表示（ZYX顺序）
EulerAngles eulerAngles = quatTransform->asEuler(EulerSequence::ZYX);
// 获取等效的旋转矩阵
Matrix3 rotationMatrix = quatTransform->asMatrix();
```

#### 2.2.6 编程提示

- **始终使用工厂方法**：避免直接实例化具体类，增加代码可维护性
- **链式调用**：利用返回智能指针的特性实现链式操作
  ```cpp
  Vector3 result = transform1->compose(*transform2)->inverse()->transform(point);
  ```
- **保持单位化**：确保四元数和矩阵保持正确的数学性质（单位四元数、正交矩阵）
- **处理特殊情况**：注意欧拉角的万向节锁问题，必要时切换表示形式

### 2.3 变换表示的具体实现

#### 2.3.1 设计考量

坐标变换的不同表示形式各有优缺点：

|表示形式|优点|缺点|主要应用场景|
|--------|----|----|------------|
|四元数|计算效率高、无奇异点、内存占用小|不直观|内部计算、姿态插值|
|旋转矩阵|直观、易于串联变换|内存占用大、数值稳定性要求高|图形渲染、物理模拟|
|欧拉角|最直观、易于理解|存在万向节锁问题|人机交互、配置文件|

根据`transform_impl.hpp`中的定义，系统实现了三种主要的变换表示类，它们都继承自`IAttitudeTransform`接口。

#### 2.3.2 四元数表示(TransformQuaternion)

四元数是一种扩展复数的代数结构，可以紧凑地表示3D旋转。

```cpp
class TransformQuaternion : public Quaternion, public IAttitudeTransform {
public:
    // 构造函数
    TransformQuaternion(double w, double x, double y, double z);
    explicit TransformQuaternion(const Quaternion& q);
    explicit TransformQuaternion(const Matrix3& m);
    explicit TransformQuaternion(const Vector3& euler, 
                                 EulerSequence sequence = EulerSequence::ZYX);
    explicit TransformQuaternion(const Vector4& axisAngle);
    
    // IAttitudeTransform接口实现
    std::shared_ptr<IAttitudeTransform> inverse() const override;
    std::shared_ptr<IAttitudeTransform> compose(const IAttitudeTransform& other) const override;
    Vector3 transform(const Vector3& v) const override;
    TransformQuaternion asQuaternion() const override { return *this; }
    TransformMatrix asMatrix() const override;
    EulerAngles asEuler(EulerSequence sequence = EulerSequence::ZYX) const override;
    Vector4 asAxisAngle() const override;
    
    // 实用方法
    static bool isValidRotationMatrix(const Matrix3& m);
    static TransformQuaternion Identity();
};
```

**关键实现细节**：

1. **四元数归一化**：
   ```cpp
   TransformQuaternion::TransformQuaternion(const Quaternion& q)
       : Quaternion(q.normalized()) 
   {
       // 确保是单位四元数
   }
   ```

2. **向量变换**：使用四元数旋转公式`q * v * q^(-1)`，v表示为纯四元数(0,v)
   ```cpp
   Vector3 TransformQuaternion::transform(const Vector3& v) const {
       // q * v * q^{-1} 计算，v被视为纯四元数(0,v)
       return Vector3(/* 计算结果 */);
   }
   ```

3. **从矩阵构造四元数**：使用稳定的算法避免数值问题
   ```cpp
   TransformQuaternion::TransformQuaternion(const Matrix3& m) {
       // 检查是否为有效旋转矩阵
       if (!isValidRotationMatrix(m)) {
           throw std::invalid_argument("不是有效的旋转矩阵");
       }
       
       // 计算四元数分量
       double trace = m(0,0) + m(1,1) + m(2,2);
       
       // 根据矩阵的迹选择不同的计算路径，提高数值稳定性
       // ...
   }
   ```

#### 2.3.3 矩阵表示(TransformMatrix)

旋转矩阵是最直观的3D旋转表示，尤其在需要串联多个变换时很有用。

```cpp
class TransformMatrix : public Matrix3, public IAttitudeTransform {
public:
    // 构造函数
    explicit TransformMatrix(const Matrix3& m);
    explicit TransformMatrix(const Quaternion& q);
    explicit TransformMatrix(const Vector3& euler, 
                            EulerSequence sequence = EulerSequence::ZYX);
    explicit TransformMatrix(const Vector4& axisAngle);
    
    // IAttitudeTransform接口实现
    std::shared_ptr<IAttitudeTransform> inverse() const override;
    std::shared_ptr<IAttitudeTransform> compose(const IAttitudeTransform& other) const override;
    Vector3 transform(const Vector3& v) const override;
    TransformQuaternion asQuaternion() const override;
    TransformMatrix asMatrix() const override { return *this; }
    EulerAngles asEuler(EulerSequence sequence = EulerSequence::ZYX) const override;
    Vector4 asAxisAngle() const override;
    
    // 实用方法
    static bool isValidRotationMatrix(const Matrix3& m);
    static TransformMatrix Identity();
};
```

**关键实现细节**：

1. **矩阵有效性检查**：确保是有效的旋转矩阵（正交且行列式为1）
   ```cpp
   bool TransformMatrix::isValidRotationMatrix(const Matrix3& m) {
       // 检查正交性：R * R^T ≈ I
       Matrix3 orthogonalityCheck = m * m.transpose();
       bool isOrthogonal = orthogonalityCheck.isApprox(Matrix3::Identity(), 1e-6);
       
       // 检查行列式：det(R) ≈ 1
       bool hasRightDeterminant = std::abs(m.determinant() - 1.0) < 1e-6;
       
       return isOrthogonal && hasRightDeterminant;
   }
   ```

2. **从四元数构造矩阵**：
   ```cpp
   TransformMatrix::TransformMatrix(const Quaternion& q) {
       // 确保四元数归一化
       Quaternion normalized = q.normalized();
       
       // 计算旋转矩阵的元素
       double w = normalized.w();
       double x = normalized.x();
       double y = normalized.y();
       double z = normalized.z();
       
       // 填充矩阵元素
       // ...
   }
   ```

3. **逆变换实现**：利用旋转矩阵的转置等于逆的特性
   ```cpp
   std::shared_ptr<IAttitudeTransform> TransformMatrix::inverse() const {
       // 旋转矩阵的逆等于其转置
       return std::make_shared<TransformMatrix>(this->transpose());
   }
   ```

#### 2.3.4 欧拉角表示(EulerAngles)

欧拉角是最容易理解的表示形式，使用三个角度按特定顺序旋转。

```cpp
class EulerAngles : public Vector3, public IAttitudeTransform {
public:
    // 构造函数
    EulerAngles(double angle1, double angle2, double angle3, 
                EulerSequence seq = EulerSequence::ZYX);
    explicit EulerAngles(const Vector3& angles, 
                         EulerSequence seq = EulerSequence::ZYX);
    explicit EulerAngles(const Quaternion& q, 
                         EulerSequence seq = EulerSequence::ZYX);
    explicit EulerAngles(const Matrix3& m, 
                         EulerSequence seq = EulerSequence::ZYX);
    explicit EulerAngles(const Vector4& axisAngle, 
                         EulerSequence seq = EulerSequence::ZYX);
    
    // 访问器
    double angle1() const { return x(); }
    double angle2() const { return y(); }
    double angle3() const { return z(); }
    EulerSequence sequence() const { return sequence_; }
    
    // IAttitudeTransform接口实现
    std::shared_ptr<IAttitudeTransform> inverse() const override;
    std::shared_ptr<IAttitudeTransform> compose(const IAttitudeTransform& other) const override;
    Vector3 transform(const Vector3& v) const override;
    TransformQuaternion asQuaternion() const override;
    TransformMatrix asMatrix() const override;
    EulerAngles asEuler(EulerSequence sequence = EulerSequence::ZYX) const override;
    Vector4 asAxisAngle() const override;
    
    // 特有方法
    EulerAngles reorder(EulerSequence newSequence) const;
    
private:
    EulerSequence sequence_ = EulerSequence::ZYX;  ///< 欧拉角序列
};
```

**关键实现细节**：

1. **从四元数提取欧拉角**：针对不同序列有不同的提取公式
   ```cpp
   EulerAngles::EulerAngles(const Quaternion& q, EulerSequence seq) 
       : sequence_(seq) 
   {
       // 归一化四元数
       Quaternion normalized = q.normalized();
       double w = normalized.w();
       double x = normalized.x();
       double y = normalized.y();
       double z = normalized.z();
       
       // 根据欧拉角序列选择不同的提取方法
       switch(seq) {
           case EulerSequence::ZYX:
               // 计算欧拉角
               // ...
               break;
           // 其他序列...
           default:
               throw std::invalid_argument("不支持的欧拉角序列");
       }
   }
   ```


#### 2.3.4 欧拉角表示(EulerAngles)

2. **欧拉角转四元数**：通过合成单轴旋转实现
   ```cpp
   TransformQuaternion EulerAngles::asQuaternion() const {
       // 根据序列不同，计算方式也不同
       double c1 = cos(x()/2), s1 = sin(x()/2); // 第一个角的余弦和正弦
       double c2 = cos(y()/2), s2 = sin(y()/2); // 第二个角的余弦和正弦
       double c3 = cos(z()/2), s3 = sin(z()/2); // 第三个角的余弦和正弦
       
       // 对于ZYX序列，计算最终四元数
       if (sequence_ == EulerSequence::ZYX) {
           return TransformQuaternion(
               c1*c2*c3 - s1*s2*s3,  // w
               c1*c2*s3 + s1*s2*c3,  // x
               c1*s2*c3 + s1*c2*s3,  // y
               s1*c2*c3 - c1*s2*s3   // z
           );
       }
       // 其他欧拉序列类似...
   }
   ```

3. **处理万向节锁问题**：
   ```cpp
   EulerAngles EulerAngles::reorder(EulerSequence newSequence) const {
       // 通过四元数中转避免直接计算
       auto quat = this->asQuaternion();
       return EulerAngles(quat, newSequence);
   }
   ```

4. **欧拉角组合变换**：通过四元数中介实现更准确的组合
   ```cpp
   std::shared_ptr<IAttitudeTransform> EulerAngles::compose(
       const IAttitudeTransform& other) const 
   {
       // 先转换为四元数再组合，避免欧拉角累积误差
       auto thisQuat = this->asQuaternion();
       auto otherQuat = other.asQuaternion();
       return thisQuat.compose(otherQuat);
   }
   ```

#### 2.3.5 不同表示形式的性能对比

| 操作 | 四元数 | 矩阵 | 欧拉角 |
|------|--------|------|--------|
| 存储空间 | 4个double (32字节) | 9个double (72字节) | 3个double+枚举 (≈32字节) |
| 向量变换 | 中等 (需要两次四元数乘法) | 最快 (单次矩阵乘法) | 最慢 (需要先转换为矩阵) |
| 组合变换 | 快速 (单次四元数乘法) | 中等 (矩阵乘法) | 最慢 (需要中间转换) |
| 插值运算 | 最优 (支持SLERP) | 困难 | 可能产生不自然结果 |
| 避免奇异点 | 是 | 是 | 否 (存在万向节锁) |

**最佳实践建议**：
- 内部计算首选四元数（数值稳定，计算高效）
- 大量向量变换时可考虑预先转为矩阵表示
- 用户交互界面使用欧拉角（直观易懂）
- 长期存储使用四元数（紧凑且精确）

### 2.4 坐标系统图(CoordinateSystemGraph)

#### 2.4.1 设计理念

坐标系统图是本模块的核心数据结构，它使用图论建模坐标系统之间的转换关系：

- **节点**：代表不同的坐标系统
- **边**：代表坐标系统之间的转换关系
- **路径**：代表从一个坐标系到另一个坐标系的转换序列

这种设计允许系统通过图遍历算法自动找到任意两个坐标系之间的转换路径，即使它们之间没有直接定义转换关系。

#### 2.4.2 关键数据结构

```cpp
class CoordinateSystemGraph {
private:
    // 存储所有变换边
    std::unordered_map<EdgeKey, Edge, EdgeKeyHash> edges_;
    
    // 邻接表加速遍历
    AdjacencyList adjacencyList_;
    
    // 路径缓存
    mutable std::unordered_map<EdgeKey, 
        std::shared_ptr<CachedPath>, 
        EdgeKeyHash> pathCache_;
    
    // 版本控制
    mutable std::atomic<uint64_t> graphVersion_{0};  // 图结构版本号
    mutable std::atomic<uint64_t> stateVersion_{0};  // 状态版本号
    
    // 互斥锁
    mutable std::mutex graphMutex_;
};
```

#### 2.4.3 双层版本控制机制

该图结构实现了精巧的双层版本控制机制：

1. **图结构版本(graphVersion_)**：
   - 当添加/删除边时递增
   - 用于检测路径缓存是否过期

2. **状态版本(stateVersion_)**：
   - 当动态转换的状态更新时递增
   - 用于检测变换缓存是否过期

这种设计带来两个关键性能优化：

```cpp
std::shared_ptr<IAttitudeTransform> CoordinateSystemGraph::getTransform(
    const CoordinateSystemIdentifier& from,
    const CoordinateSystemIdentifier& to) const 
{
    // 统一路径查询
    auto path = findOrCreatePath(from, to);
    if (!path) {
        throw std::runtime_error("No transform path found");
    }

    // 检查缓存 - 节省重复计算
    if (path->cachedTransform && 
        (!path->hasDynamic || path->stateVersion == stateVersion_)) {
        return path->cachedTransform;  // 使用缓存的变换
    }

    // 带版本检查的变换计算
    auto result = TransformComposer::compose(
        path->edges, edges_, path->graphVersion, graphVersion_);

    // 更新缓存
    path->cachedTransform = result;
    path->stateVersion = stateVersion_;

    return result;
}
```

这种机制确保了：
- 静态路径只计算一次，后续直接使用缓存
- 动态路径只在状态更新时重新计算
- 图结构更新时清理过时缓存

#### 2.4.4 边结构设计

`Edge`结构支持两种变换类型：

```cpp
struct Edge {
    // 静态变换
    std::shared_ptr<IAttitudeTransform> transform;
    
    // 动态变换生成器
    std::function<std::shared_ptr<IAttitudeTransform>(
        const CoordinateSystemIdentifier&,
        const CoordinateSystemIdentifier&)> generator;
        
    bool isDynamic;                 // 是否为动态变换
    bool isBidirectional{true};     // 是否为双向变换
};
```

这种设计允许：

1. **静态变换**：一次性创建后不再变化的转换，如NED到ENU的固定变换

2. **动态变换**：依赖于系统当前状态的变换，如机体系到NED系的变换
   ```cpp
   // 使用示例
   auto bodyToNed = [](
       const CoordinateSystemIdentifier& from,
       const CoordinateSystemIdentifier& to) 
   {
       auto& stateManager = states::StateManager::getInstance();
       auto vehicleId = from.vehicleId.value_or(0);
       auto attitude = stateManager.getAttitude(vehicleId);
       return IAttitudeTransform::FromQuaternion(attitude);
   };
   ```

#### 2.4.5 路径查找实现

路径查找委托给了`PathFinder`类，它使用广度优先搜索(BFS)算法：

```cpp
std::shared_ptr<CachedPath> PathFinder::findPath(
    const CoordinateSystemIdentifier& from,
    const CoordinateSystemIdentifier& to,
    const AdjacencyList& adjacency,
    uint64_t currentVersion)
{
    std::unordered_map<CoordinateSystemIdentifier, 
        CoordinateSystemIdentifier> prev;
    std::queue<CoordinateSystemIdentifier> queue;
    
    queue.push(from);
    prev[from] = from;
    
    bool found = false;
    while (!queue.empty() && !found) {
        auto current = queue.front();
        queue.pop();
        
        // 使用邻接表加速遍历
        auto it = adjacency.find(current);
        if (it == adjacency.end()) continue;
        
        for (const auto& neighbor : it->second) {
            if (prev.find(neighbor) == prev.end()) {
                prev[neighbor] = current;
                queue.push(neighbor);
                
                if (neighbor == to) {
                    found = true;
                    break;
                }
            }
        }
    }
    
    if (!found) {
        return nullptr;  // 没找到路径
    }
    
    // 构建路径
    auto path = std::make_shared<CachedPath>();
    path->graphVersion = currentVersion;
    
    auto current = to;
    while (current != from) {
        auto previous = prev[current];
        path->edges.insert(path->edges.begin(), 
            std::make_pair(previous, current));
        current = previous;
    }
    
    return path;
}
```

**算法亮点**：
- 使用邻接表结构加速图遍历
- 通过prev映射记录路径，支持反向重建
- 返回带版本号的缓存路径对象

#### 2.4.6 使用示例

```cpp
// 创建图实例
CoordinateSystemGraph graph;

// 添加静态变换边
auto nedToEnuTransform = IAttitudeTransform::FromEuler(
    Vector3(M_PI, 0, M_PI/2), EulerSequence::ZYX);

Edge staticEdge;
staticEdge.transform = nedToEnuTransform;
staticEdge.isDynamic = false;
graph.addEdge({CoordinateSystems::NED, CoordinateSystems::ENU}, staticEdge);

// 添加动态变换边
Edge dynamicEdge;
dynamicEdge.generator = bodyToNedGenerator;
dynamicEdge.isDynamic = true;
graph.addEdge(
    {CoordinateSystems::BODY(1), CoordinateSystems::NED}, 
    dynamicEdge);

// 获取变换
try {
    // 即使没有直接定义BODY到ENU的变换，系统也能找到路径
    auto transform = graph.getTransform(
        CoordinateSystems::BODY(1), CoordinateSystems::ENU);
    
    // 应用变换
    Vector3 pointInBody(1, 0, 0);
    Vector3 pointInEnu = transform->transform(pointInBody);
} catch (const std::exception& e) {
    std::cout << "无法找到变换路径: " << e.what() << std::endl;
}

// 更新动态变换状态
graph.updateDynamicTransforms();
```

### 2.5 变换组合器(TransformComposer)

#### 2.5.1 设计理念

`TransformComposer`类负责将多个基本变换组合为一个复合变换。该类解决的核心问题是：

1. 如何高效组合变换路径上的所有边
2. 如何处理静态和动态变换的混合
3. 如何保证变换组合的正确性

#### 2.5.2 实现细节

```cpp
std::shared_ptr<IAttitudeTransform> TransformComposer::compose(
    const std::vector<EdgeKey>& path,
    const std::unordered_map<EdgeKey, Edge, EdgeKeyHash>& edges,
    uint64_t cachedVersion,
    uint64_t currentVersion)
{
    // 检查版本一致性
    if (cachedVersion != currentVersion) {
        throw std::logic_error("Stale path detected");
    }

    if (path.empty()) {
        return std::make_shared<TransformQuaternion>(Quaternion::Identity());
    }

    // 计算第一个变换
    auto result = getTransform(edges.at(path[0]), path[0]);

    // 组合后续变换
    for (size_t i = 1; i < path.size(); ++i) {
        auto transform = getTransform(edges.at(path[i]), path[i]);
        result = result->compose(*transform);
    }

    return result;
}
```

核心方法`getTransform`根据边的类型返回相应的变换：

```cpp
std::shared_ptr<IAttitudeTransform> TransformComposer::getTransform(
    const Edge& edge,
    const EdgeKey& key)
{
    if (edge.isDynamic) {
        return edge.generator(key.first, key.second);
    }
    return edge.transform;
}
```

#### 2.5.3 实现亮点

1. **版本检查**：
   首先检查路径版本是否与当前图版本一致，防止使用过时的路径

2. **空路径处理**：
   空路径时返回单位变换，确保API行为一致性

3. **组合优化**：
   从左到右顺序组合变换，确保数学上的正确性

4. **动态/静态统一接口**：
   `getTransform`方法提供统一接口，屏蔽底层实现差异

#### 2.5.4 性能考量

1. **避免重复计算**：组合变换时，中间结果会被保留

2. **数值稳定性**：每个基本变换都保证数值稳定（如四元数归一化）

3. **错误处理**：对无效路径抛出异常，避免静默失败

### 2.6 坐标系统管理器(CoordinateSystemManager)

#### 2.6.1 设计理念

`CoordinateSystemManager`是整个坐标系统模块的门面(Facade)，它整合了其他组件，提供了一个统一、简洁的接口。该管理器采用单例模式，确保全局只有一个实例管理所有坐标系统。

#### 2.6.2 核心功能

1. **坐标系统生命周期管理**：创建、获取和销毁坐标系统实例
2. **变换关系注册**：添加和维护坐标系统间的转换关系
3. **变换查询**：提供高级接口获取任意两个坐标系间的变换
4. **全局访问点**：使用单例模式提供全局唯一访问点

#### 2.6.3 实现细节

```cpp
class CoordinateSystemManager {
public:
    // 单例访问
    static CoordinateSystemManager& getInstance() {
        static CoordinateSystemManager instance;
        return instance;
    }

    // 获取或创建坐标系统
    std::shared_ptr<ICoordinateSystem> getCoordinateSystem(
        CoordinateSystemType type,
        std::optional<uint32_t> vehicleId = std::nullopt) 
    {
        CoordinateSystemIdentifier id{type, vehicleId};
        auto it = systems_.find(id);
        if (it == systems_.end()) {
            auto system = createCoordinateSystem(type, vehicleId);
            systems_[id] = system;
            return system;
        }
        return it->second;
    }

    // 获取变换（委托给图结构）
    std::shared_ptr<IAttitudeTransform> getTransform(
        const CoordinateSystemIdentifier& from,
        const CoordinateSystemIdentifier& to) const 
    {
        return transformGraph_.getTransform(from, to);
    }

    // 注册静态变换
    void registerBaseTransform(
        const CoordinateSystemIdentifier& from,
        const CoordinateSystemIdentifier& to,
        std::shared_ptr<IAttitudeTransform> transform,
        bool forceUpdate = false);

    // 注册动态变换
    void registerDynamicTransform(
        const CoordinateSystemIdentifier& from,
        const CoordinateSystemIdentifier& to,
        TransformGenerator generator,
        bool forceUpdate = false);

private:
    // 私有构造函数，防止外部实例化
    CoordinateSystemManager() = default;
    
    // 存储所有坐标系统
    std::unordered_map<CoordinateSystemIdentifier,
        std::shared_ptr<ICoordinateSystem>> systems_;
        
    // 变换图结构
    CoordinateSystemGraph transformGraph_;
};
```

#### 2.6.4 使用示例

```cpp
// 获取管理器实例
auto& manager = CoordinateSystemManager::getInstance();

// 注册基本变换关系
auto nedToEcefTransform = IAttitudeTransform::FromQuaternion(someQuaternion);
manager.registerBaseTransform(
    CoordinateSystems::NED, 
    CoordinateSystems::ECEF, 
    nedToEcefTransform);

// 注册动态变换
manager.registerDynamicTransform(
    CoordinateSystems::BODY(1), 
    CoordinateSystems::NED, 
    bodyToNedGenerator);

// 获取坐标系统
auto nedSystem = manager.getCoordinateSystem(CoordinateSystemType::NED);
auto bodySystem = manager.getCoordinateSystem(
    CoordinateSystemType::BODY, 1); // 飞行器1的机体系

// 获取变换并应用
auto bodyToEcefTransform = manager.getTransform(
    bodySystem->getIdentifier(), 
    CoordinateSystems::ECEF);
    
Vector3 pointInBody(1, 2, 3);
Vector3 pointInEcef = bodyToEcefTransform->transform(pointInBody);
```

#### 2.6.5 设计模式应用

1. **单例模式**：确保全局只有一个坐标系统管理器实例

2. **工厂模式**：通过`createCoordinateSystem`方法创建坐标系统实例

3. **门面模式**：提供简化的高级接口，隐藏底层复杂实现

4. **代理模式**：对`transformGraph_`的方法调用进行封装和控制

## 3. 最佳实践与性能优化

### 3.1 常用坐标系统转换

#### 3.1.1 预定义的转换生成器

为了方便使用，系统预定义了一些常用的转换生成器：

```cpp
/**
 * @brief 机体系到NED系的转换生成器
 */
inline auto bodyToNed = [](
    const CoordinateSystemIdentifier& from,
    const CoordinateSystemIdentifier& to) 
{
    auto& stateManager = states::StateManager::getInstance();
    auto vehicleId = from.vehicleId.value_or(0);
    // 获取当前飞行器姿态
    return IAttitudeTransform::Identity(); // 示例，实际应获取真实姿态
};

/**
 * @brief NED系到ENU系的转换生成器
 */
inline auto nedToEnu = [](
    const CoordinateSystemIdentifier& from,
    const CoordinateSystemIdentifier& to) 
{
    // 静态变换矩阵，但为了统一接口使用生成器形式
    static const auto transform = IAttitudeTransform::Identity(); // 简化示例
    return transform;
};
```

#### 3.1.2 常见坐标系统关系

| 从坐标系 | 到坐标系 | 变换类型 | 变换特性 |
|---------|--------|---------|---------|
| BODY | NED | 动态 | 依赖于飞行器姿态 |
| NED | ENU | 静态 | 固定90度偏航旋转 |
| ECEF | ECI | 动态 | 依赖于地球自转 |
| NED | ECEF | 动态 | 依赖于位置（经纬度） |
| LAUNCH | NED | 静态 | 依赖于发射点位置 |

### 3.2 性能优化技巧

#### 3.2.1 缓存管理

系统使用双层缓存提高性能：

1. **路径缓存**：避免重复的图遍历
2. **变换缓存**：避免重复的变换计算

关键在于正确管理缓存的生命周期：

```cpp
// 检查缓存是否有效
if (path->cachedTransform && 
    (!path->hasDynamic || path->stateVersion == stateVersion_)) {
    return path->cachedTransform;  // 使用缓存
}

// 重新计算并更新缓存
auto result = TransformComposer::compose(...);
path->cachedTransform = result;
path->stateVersion = stateVersion_;
```

#### 3.2.2 变换组合优化

对于多步变换，系统会预先组合为单一变换：

```cpp
// 不推荐：单独应用每个变换
Vector3 result = v;
for (auto& transform : transforms) {
    result = transform->transform(result);
}

// 推荐：先组合变换，再一次应用
auto combinedTransform = composeAll(transforms);
Vector3 result = combinedTransform->transform(v);
```

这样可以大幅减少计算量，特别是处理大量向量时。

#### 3.2.3 数据局部性优化

系统中的数据结构设计考虑了缓存友好性：

1. **邻接表**：改进图遍历性能
2. **向量预分配**：减少内存重分配
3. **内联小函数**：减少函数调用开销

### 3.3 常见陷阱与解决方案

#### 3.3.1 万向节锁问题

在使用欧拉角表示时，当第二个旋转角接近±90度时，会出现万向节锁：

```cpp
// 危险：接近万向节锁
EulerAngles angles(0, M_PI/2 - 0.001, 0, EulerSequence::ZYX);

// 解决方案：使用四元数表示
auto transform = IAttitudeTransform::FromEuler(angles.asVector3(), angles.sequence());
// 现在可以安全地进行插值、组合等操作
```

#### 3.3.2 坐标系统定义不一致

不同系统可能使用不同的坐标系约定：

```cpp
// 问题：混合使用不同系统
// OpenGL使用右手坐标系，Y轴向上
// 航空常用NED系，Z轴向下

// 解决方案：定义明确的转换
auto openglToNed = IAttitudeTransform::FromEuler(
    Vector3(M_PI, 0, 0), EulerSequence::XYZ);
```

#### 3.3.3 变换顺序错误

旋转变换的顺序至关重要：

```cpp
// 错误：顺序反了
auto transform = transform1->compose(*transform2);  // 先transform2，再transform1

// 正确：明确指定顺序
// 要先应用transform1，再应用transform2：
auto transform = transform2->compose(*transform1);
// 或者使用更明确的命名:
auto transform = secondTransform->compose(*firstTransform);
```

## 4. 实际应用案例

### 4.1 多无人机编队应用

```cpp
// 初始化坐标系统管理器
auto& manager = CoordinateSystemManager::getInstance();

// 注册每个无人机的机体系到NED系的变换
for (uint32_t uavId = 1; uavId <= 3; uavId++) {
    manager.registerDynamicTransform(
        {CoordinateSystemType::BODY, uavId},
        {CoordinateSystemType::NED, std::nullopt},
        [uavId](const auto& from, const auto& to) {
            // 从状态管理器获取姿态
            auto& stateManager = states::StateManager::getInstance();
            auto attitude = stateManager.getAttitude(uavId);
            return IAttitudeTransform::FromQuaternion(attitude);
        }
    );
}

// 计算从无人机1到无人机2的相对变换
auto uav1ToUav2 = manager.getTransform(
    {CoordinateSystemType::BODY, 1},
    {CoordinateSystemType::BODY, 2}
);

// 获取无人机1在无人机2坐标系中的位置
Vector3 uav1PosInNed = getUavPosition(1);
Vector3 uav1PosInUav2 = uav1ToUav2->transform(uav1PosInNed);
```

### 4.2 传感器融合应用

```cpp
// 定义相机坐标系
auto cameraId = CoordinateSystemIdentifier(CoordinateSystemType::CAMERA, 1);

// 注册相机到机体系的静态变换（安装偏差）
Matrix3 installationMatrix = calculateInstallationMatrix();
manager.registerBaseTransform(
    cameraId,
    {CoordinateSystemType::BODY, 1},
    std::make_shared<TransformMatrix>(installationMatrix)
);

// 处理相机观测到的目标
Vector3 targetInCamera = cameraDetectTarget();
// 转换到NED坐标系
auto cameraToNed = manager.getTransform(cameraId, CoordinateSystems::NED);
Vector3 targetInNed = cameraToNed->transform(targetInCamera);

// 将目标传递给导航模块
navigator.updateTarget(targetInNed);
```

## 5. 扩展与自定义

### 5.1 添加新的坐标系类型

1. 更新CoordinateSystemType枚举：

```cpp
enum class CoordinateSystemType {
    // 现有类型
    LAUNCH, ECEF, ECI, NED, ENU, BODY,
    // 新增类型
    CAMERA,  // 相机坐标系
    LIDAR,   // 激光雷达坐标系
    CUSTOM   // 自定义坐标系
};
```


1. 创建新的坐标变换生成器：

```cpp
/**
 * @brief 自定义坐标系到NED系的变换生成器
 */
inline auto customToNed = [](
    const CoordinateSystemIdentifier& from,
    const CoordinateSystemIdentifier& to) 
{
    auto& stateManager = states::StateManager::getInstance();
    auto vehicleId = from.vehicleId.value_or(0);
    
    // 获取必要的状态数据
    // 例如位置、姿态或其他自定义参数
    auto customState = stateManager.getState<CustomStateType>(vehicleId);
    
    // 基于状态构建变换
    // 这里可以根据具体应用场景实现转换逻辑
    Quaternion rotation = customState.getOrientation();
    return std::make_shared<TransformQuaternion>(rotation);
};
```

3. 在管理器中注册新的坐标系统和变换关系：

```cpp
// 获取管理器实例
auto& manager = CoordinateSystemManager::getInstance();

// 创建自定义坐标系标识符
CoordinateSystemIdentifier customId{CoordinateSystemType::CUSTOM, vehicleId};

// 注册动态变换
manager.registerDynamicTransform(
    customId,
    CoordinateSystemIdentifier{CoordinateSystemType::NED},
    customToNed
);
```

4. 创建辅助函数或命名空间常量进行封装：

```cpp
namespace CoordinateSystems {
    // 添加到之前定义的命名空间中
    inline CoordinateSystemIdentifier CUSTOM(uint32_t vehicleId = 0) {
        return {CoordinateSystemType::CUSTOM, vehicleId ? std::optional<uint32_t>(vehicleId) : std::nullopt};
    }
}
```

### 5.2 自定义变换实现

除了使用系统提供的变换类型外，你还可以实现自己的变换类。

#### 5.2.1 创建自定义变换类

1. 实现IAttitudeTransform接口：

```cpp
/**
 * @brief 基于缩放四元数的特殊变换
 * @details 这是一个示例自定义变换，扩展了标准四元数旋转，添加了缩放因子
 */
class ScaledTransform : public IAttitudeTransform {
public:
    // 构造函数
    ScaledTransform(const Quaternion& rotation, double scale)
        : rotation_(rotation.normalized()), scale_(scale) {}
    
    // IAttitudeTransform接口实现
    std::shared_ptr<IAttitudeTransform> inverse() const override {
        return std::make_shared<ScaledTransform>(
            rotation_.conjugate(), 
            1.0 / scale_);
    }
    
    std::shared_ptr<IAttitudeTransform> compose(
        const IAttitudeTransform& other) const override 
    {
        // 如果other也是ScaledTransform，可以直接组合
        if (auto* scaled = dynamic_cast<const ScaledTransform*>(&other)) {
            return std::make_shared<ScaledTransform>(
                rotation_ * scaled->rotation_,
                scale_ * scaled->scale_);
        }
        
        // 否则将other转为四元数后组合
        auto otherQuat = other.asQuaternion();
        return std::make_shared<ScaledTransform>(
            rotation_ * otherQuat,
            scale_);
    }
    
    Vector3 transform(const Vector3& v) const override {
        // 先缩放再旋转
        return rotation_._transformVector(v * scale_);
    }
    
    TransformQuaternion asQuaternion() const override {
        // 注意：舍弃缩放信息
        return TransformQuaternion(rotation_);
    }
    
    TransformMatrix asMatrix() const override {
        // 创建包含缩放的矩阵
        Matrix3 rotMat = rotation_.toRotationMatrix();
        return TransformMatrix(rotMat * scale_);
    }
    
    EulerAngles asEuler(EulerSequence sequence = EulerSequence::ZYX) const override {
        // 注意：舍弃缩放信息
        return EulerAngles(rotation_, sequence);
    }
    
    Vector4 asAxisAngle() const override {
        // 注意：舍弃缩放信息
        // 实现轴角转换逻辑...
        Vector4 axisAngle;
        // 填充轴角数据
        return axisAngle;
    }
    
    // 自定义方法
    double getScale() const { return scale_; }

private:
    Quaternion rotation_;   ///< 旋转部分
    double scale_;          ///< 缩放因子
};
```

#### 5.2.2 使用自定义变换

在变换注册和使用时：

```cpp
// 创建自定义变换
auto scaledTransform = std::make_shared<ScaledTransform>(
    Quaternion::FromAngleAxis(M_PI/4, Vector3(0,0,1)), // 45度Z轴旋转
    2.0  // 2倍缩放
);

// 在坐标系管理器中注册
manager.registerBaseTransform(
    CoordinateSystems::CUSTOM(),
    CoordinateSystems::NED,
    scaledTransform
);

// 使用变换
Vector3 pointInCustom(1, 1, 0);
auto transform = manager.getTransform(
    CoordinateSystems::CUSTOM(),
    CoordinateSystems::NED
);
Vector3 pointInNed = transform->transform(pointInCustom);
// 结果应该是旋转45度并放大2倍: (0, 2√2, 0)

// 检查缩放因子（如果需要）
if (auto* scaled = dynamic_cast<ScaledTransform*>(transform.get())) {
    double scale = scaled->getScale();
    std::cout << "Transform scale: " << scale << std::endl;
}
```

### 5.3 扩展坐标系图

随着项目发展，你可能需要扩展坐标系统图的功能。

#### 5.3.1 添加新的边属性

如果需要为边添加新属性（如精度信息、时间戳等），可以修改Edge结构：

```cpp
struct ExtendedEdge : public Edge {
    double accuracy{1.0};          // 变换精度指标
    uint64_t timestamp{0};         // 上次更新时间戳
    std::string source{"default"}; // 数据来源
    
    // 从基类构造
    ExtendedEdge(const Edge& base) 
        : Edge(base) {}
    
    // 完整构造函数
    ExtendedEdge(std::shared_ptr<IAttitudeTransform> t, 
                bool isDyn, 
                double acc,
                const std::string& src)
        : Edge{t, nullptr, isDyn}, 
          accuracy(acc),
          source(src) {}
};
```

#### 5.3.2 实现自定义路径查找算法

对于特殊需求（如最高精度路径、最小跳数路径），可以实现自定义路径查找：

```cpp
class AccuracyPathFinder {
public:
    /**
     * @brief 查找最高精度的路径
     */
    static std::shared_ptr<CachedPath> findMostAccuratePath(
        const CoordinateSystemIdentifier& from,
        const CoordinateSystemIdentifier& to,
        const std::unordered_map<EdgeKey, ExtendedEdge, EdgeKeyHash>& edges,
        const AdjacencyList& adjacency,
        uint64_t currentVersion) 
    {
        // Dijkstra算法实现，以精度的倒数作为边权重
        // 这里我们只给出概念实现
        
        // 初始化距离映射和优先队列
        std::unordered_map<CoordinateSystemIdentifier, double> accuracy;
        std::unordered_map<CoordinateSystemIdentifier, 
            CoordinateSystemIdentifier> prev;
        
        // 使用优先队列
        using QueueItem = std::pair<double, CoordinateSystemIdentifier>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, 
            std::greater<QueueItem>> queue;
            
        // 初始节点
        accuracy[from] = 1.0;  // 初始精度为1
        queue.push({1.0, from});
        
        // Dijkstra主循环
        while (!queue.empty()) {
            auto [currentAccuracy, current] = queue.top();
            queue.pop();
            
            // 找到目标
            if (current == to) break;
            
            // 如果已经找到更好的路径，跳过
            if (currentAccuracy < accuracy[current]) continue;
            
            // 遍历邻居
            auto it = adjacency.find(current);
            if (it == adjacency.end()) continue;
            
            for (const auto& neighbor : it->second) {
                // 获取边
                EdgeKey edgeKey{current, neighbor};
                const auto& edge = edges.at(edgeKey);
                
                // 计算新精度（相乘，因为是概率）
                double newAccuracy = currentAccuracy * edge.accuracy;
                
                // 如果找到更高精度路径，更新
                if (accuracy.find(neighbor) == accuracy.end() || 
                    newAccuracy > accuracy[neighbor]) {
                    
                    accuracy[neighbor] = newAccuracy;
                    prev[neighbor] = current;
                    queue.push({newAccuracy, neighbor});
                }
            }
        }
        
        // 创建路径（类似标准PathFinder）
        auto path = std::make_shared<CachedPath>();
        path->graphVersion = currentVersion;
        
        // 如果没有找到路径
        if (prev.find(to) == prev.end()) {
            return nullptr;
        }
        
        // 构建路径
        auto current = to;
        while (current != from) {
            auto previous = prev[current];
            path->edges.insert(path->edges.begin(), 
                std::make_pair(previous, current));
            current = previous;
        }
        
        return path;
    }
};
```

## 6. 调试与故障排除

### 6.1 跟踪和记录转换路径

在开发和调试过程中，了解实际使用了哪些转换路径非常有用。以下是一个简单的路径跟踪装饰器的实现：

```cpp
/**
 * @brief 坐标系统图装饰器，用于跟踪转换路径
 * @details 包装CoordinateSystemGraph，在每次转换前后添加日志
 */
class TracingGraphDecorator {
public:
    TracingGraphDecorator(CoordinateSystemGraph& graph) : graph_(graph) {}
    
    std::shared_ptr<IAttitudeTransform> getTransform(
        const CoordinateSystemIdentifier& from,
        const CoordinateSystemIdentifier& to) 
    {
        std::cout << "Transform request: " 
                  << to_string(from) << " -> " 
                  << to_string(to) << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = graph_.getTransform(from, to);
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> duration = end - start;
        
        std::cout << "Transform completed in " 
                  << duration.count() << " ms" << std::endl;
                  
        return result;
    }
    
private:
    CoordinateSystemGraph& graph_;
    
    std::string to_string(const CoordinateSystemIdentifier& id) {
        std::stringstream ss;
        ss << getCoordinateSystemTypeName(id.type);
        if (id.vehicleId) {
            ss << "(Vehicle " << *id.vehicleId << ")";
        }
        return ss.str();
    }
    
    std::string getCoordinateSystemTypeName(CoordinateSystemType type) {
        switch(type) {
            case CoordinateSystemType::NED: return "NED";
            case CoordinateSystemType::ENU: return "ENU";
            case CoordinateSystemType::BODY: return "BODY";
            case CoordinateSystemType::ECEF: return "ECEF";
            case CoordinateSystemType::ECI: return "ECI";
            case CoordinateSystemType::LAUNCH: return "LAUNCH";
            default: return "UNKNOWN";
        }
    }
};
```

使用方法：

```cpp
// 创建装饰器
TracingGraphDecorator tracer(manager.getGraph());

// 使用装饰器进行转换
auto transform = tracer.getTransform(
    CoordinateSystems::BODY(1),
    CoordinateSystems::NED
);

// 日志输出示例:
// Transform request: BODY(Vehicle 1) -> NED
// Transform completed in 0.235 ms
```

### 6.2 诊断常见问题

#### 6.2.1 找不到变换路径

当系统无法找到从源坐标系到目标坐标系的路径时，常见原因包括：

1. **缺少注册的变换**：检查是否注册了必要的变换关系

   ```cpp
   // 诊断工具：输出所有注册的变换关系
   void printAllTransforms(const CoordinateSystemGraph& graph) {
       for (const auto& [edge, transform] : graph.getEdges()) {
           std::cout << to_string(edge.first) << " -> " 
                     << to_string(edge.second) << ": "
                     << (transform.isDynamic ? "Dynamic" : "Static")
                     << std::endl;
       }
   }
   ```

2. **坐标系标识符不匹配**：检查标识符是否正确创建

   ```cpp
   // 常见错误：忘记指定vehicleId
   auto transform = manager.getTransform(
       CoordinateSystemType::BODY,  // 错误！这只指定了类型，没有指定vehicleId
       CoordinateSystemType::NED
   );
   
   // 正确做法
   auto transform = manager.getTransform(
       CoordinateSystemIdentifier{CoordinateSystemType::BODY, 1},
       CoordinateSystemIdentifier{CoordinateSystemType::NED}
   );
   
   // 或使用辅助函数
   auto transform = manager.getTransform(
       CoordinateSystems::BODY(1),
       CoordinateSystems::NED
   );
   ```

3. **断开的子图**：确保图中所有坐标系都连接到主图

   ```cpp
   // 检查坐标系是否孤立
   bool isIsolated(const CoordinateSystemIdentifier& id,
                   const AdjacencyList& adjacency) {
       return adjacency.find(id) == adjacency.end() ||
              adjacency.at(id).empty();
   }
   ```

#### 6.2.2 变换结果不正确

如果变换生成但结果不符合预期：

1. **变换顺序问题**：确认变换应用顺序正确

   ```cpp
   // 常见错误：变换顺序颠倒
   auto transform = transform1->compose(*transform2);
   // 实际执行顺序：先transform2，再transform1
   
   // 诊断工具：打印变换组合顺序
   void traceCompose(const IAttitudeTransform& first, 
                     const IAttitudeTransform& second) {
       std::cout << "Composing transforms: " 
                 << transformTypeToString(second)
                 << " THEN " 
                 << transformTypeToString(first) 
                 << std::endl;
   }
   ```

2. **四元数规范化问题**：检查四元数是否正确归一化

   ```cpp
   // 诊断工具：检查四元数范数
   void checkQuaternion(const Quaternion& q) {
       double norm = q.norm();
       if (std::abs(norm - 1.0) > 1e-6) {
           std::cout << "Warning: Quaternion not normalized! "
                     << "Norm: " << norm << std::endl;
       }
   }
   ```

3. **坐标系定义不一致**：确保坐标轴定义一致

   ```cpp
   // 诊断工具：打印坐标系基向量
   void printBasisVectors(const IAttitudeTransform& transform) {
       Vector3 x = transform.transform(Vector3(1, 0, 0));
       Vector3 y = transform.transform(Vector3(0, 1, 0));
       Vector3 z = transform.transform(Vector3(0, 0, 1));
       
       std::cout << "X basis: " << x.transpose() << std::endl;
       std::cout << "Y basis: " << y.transpose() << std::endl;
       std::cout << "Z basis: " << z.transpose() << std::endl;
   }
   ```

#### 6.2.3 动态变换未更新

如果动态变换没有反映最新状态：

1. **忘记调用updateDynamicTransforms**：确保状态更新后调用此方法

   ```cpp
   // 正确的更新顺序
   stateManager.updateState(vehicleId, newState);
   manager.updateDynamicTransforms();
   ```

2. **变换生成器逻辑错误**：验证变换生成器逻辑

   ```cpp
   // 诊断工具：包装变换生成器，添加日志
   auto debugBodyToNed = [originalGenerator](
       const CoordinateSystemIdentifier& from,
       const CoordinateSystemIdentifier& to) 
   {
       auto transform = originalGenerator(from, to);
       std::cout << "Generated BodyToNed transform: " 
                 << quaternionToString(transform->asQuaternion())
                 << std::endl;
       return transform;
   };
   ```

### 6.3 单元测试策略

有效的单元测试对确保坐标系统的正确性至关重要。以下是一些推荐的测试策略：

#### 6.3.1 基本变换测试

测试每种表示形式的基本变换操作：

```cpp
TEST(AttitudeTransformTest, IdentityTransform) {
    auto identity = IAttitudeTransform::Identity();
    Vector3 v(1, 2, 3);
    Vector3 result = identity->transform(v);
    
    ASSERT_NEAR(v.x(), result.x(), 1e-10);
    ASSERT_NEAR(v.y(), result.y(), 1e-10);
    ASSERT_NEAR(v.z(), result.z(), 1e-10);
}

TEST(AttitudeTransformTest, ComposedTransformEquivalence) {
    // 创建两个90度旋转
    auto rotX = IAttitudeTransform::FromEuler(
        Vector3(M_PI/2, 0, 0), EulerSequence::XYZ);
    auto rotY = IAttitudeTransform::FromEuler(
        Vector3(0, M_PI/2, 0), EulerSequence::XYZ);
    
    // 组合方式1: 先X后Y
    auto combined1 = rotY->compose(*rotX);
    
    // 方式2: 直接创建XY组合旋转
    auto combined2 = IAttitudeTransform::FromEuler(
        Vector3(M_PI/2, M_PI/2, 0), EulerSequence::XYZ);
    
    // 测试点
    Vector3 v(1, 0, 0);
    Vector3 result1 = combined1->transform(v);
    Vector3 result2 = combined2->transform(v);
    
    // 结果应该接近
    ASSERT_NEAR(result1.x(), result2.x(), 1e-10);
    ASSERT_NEAR(result1.y(), result2.y(), 1e-10);
    ASSERT_NEAR(result1.z(), result2.z(), 1e-10);
}
```

#### 6.3.2 坐标系图测试

测试坐标系统图的路径查找和变换：

```cpp
TEST(CoordinateSystemGraphTest, PathFinding) {
    CoordinateSystemGraph graph;
    
    // 创建测试坐标系
    auto a = CoordinateSystemIdentifier(CoordinateSystemType::BODY, 1);
    auto b = CoordinateSystemIdentifier(CoordinateSystemType::NED);
    auto c = CoordinateSystemIdentifier(CoordinateSystemType::ECEF);
    
    // 添加测试边
    Edge edgeAB;
    edgeAB.transform = IAttitudeTransform::FromEuler(
        Vector3(0.1, 0.2, 0.3));
    edgeAB.isDynamic = false;
    graph.addEdge({a, b}, edgeAB);
    
    Edge edgeBC;
    edgeBC.transform = IAttitudeTransform::FromEuler(
        Vector3(0.4, 0.5, 0.6));
    edgeBC.isDynamic = false;
    graph.addEdge({b, c}, edgeBC);
    
    // 直接路径
    auto transformAB = graph.getTransform(a, b);
    ASSERT_NE(transformAB, nullptr);
    
    // 多步路径
    auto transformAC = graph.getTransform(a, c);
    ASSERT_NE(transformAC, nullptr);
    
    // 验证多步路径等于组合路径
    Vector3 v(1, 0, 0);
    Vector3 directResult = transformAB->compose(*graph.getTransform(b, c))->transform(v);
    Vector3 pathResult = transformAC->transform(v);
    
    ASSERT_NEAR(directResult.x(), pathResult.x(), 1e-10);
    ASSERT_NEAR(directResult.y(), pathResult.y(), 1e-10);
    ASSERT_NEAR(directResult.z(), pathResult.z(), 1e-10);
}
```

#### 6.3.3 边缘情况测试

测试各种边缘情况和错误处理：

```cpp
TEST(CoordinateSystemGraphTest, NonExistentPath) {
    CoordinateSystemGraph graph;
    
    auto a = CoordinateSystemIdentifier(CoordinateSystemType::BODY, 1);
    auto b = CoordinateSystemIdentifier(CoordinateSystemType::NED);
    
    // 未添加任何边
    ASSERT_THROW(graph.getTransform(a, b), std::runtime_error);
}

TEST(AttitudeTransformTest, EulerAnglesGimbalLock) {
    // 创建接近万向节锁的欧拉角
    Vector3 eulerAngles(0.1, M_PI/2 - 0.001, 0.2);
    
    // 转为四元数再转回欧拉角，检查与原值的差异
    auto transform = IAttitudeTransform::FromEuler(eulerAngles);
    EulerAngles result = transform->asEuler();
    
    // 注意：在接近万向节锁时，第一个和第三个角可能有较大变化
    // 但它们的组合效果应该相似
}
```
