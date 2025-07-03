# 坐标与张量系统设计方案

本文档详细描述了GNC项目中坐标与张量系统的三层分离式架构设计方案。

## 核心设计哲学

- **分层与解耦**: 严格分离纯数学计算、抽象接口和应用层实现，遵循依赖倒置原则。
- **依赖注入**: 上层模块或应用逻辑通过构造函数或方法参数，将依赖的抽象服务（如`ITransformProvider`）注入到需要它的对象中，而不是让对象内部自己创建或访问全局实例。
- **接口而非实现**: 所有组件应依赖于抽象接口，而非具体实现，以提高模块化、可测试性和可扩展性。

---

## 架构分层详解

### 第一层：纯数学核心库

**位置**: `include/math/`

这一层是整个系统的数学基石，不包含任何与GNC项目状态或坐标系概念相关的逻辑。

#### 1. `math/tensor/tensor.hpp` (新)

- **目的**: 统一项目中表示向量、矩阵等数学对象的类型，并直接利用 `Eigen` 库的强大功能。
- **设计要点**:
    - **直接使用 Eigen**: 项目将直接使用 `Eigen` 库来处理所有线性代数运算。这避免了重复造轮子，并能利用 `Eigen` 的高性能和丰富功能。
    - **类型别名 (Type Aliases)**: 为了代码的清晰性和未来的可维护性，我们将创建一组类型别名，将项目特定的数学类型（如 `Vector3d`, `Matrix3d`）映射到对应的 `Eigen` 类型。
        ```cpp
        // 在例如 math/types.hpp 或类似的核心头文件中定义
        #include <Eigen/Dense>

        namespace gnc::math {
            using Vector3d = Eigen::Vector3d;
            using Matrix3d = Eigen::Matrix3d;
            // ... 其他需要的类型
        }
        ```
    - **优势**:
        - **功能完备**: 无需手动实现复杂的数学运算。
        - **高性能**: `Eigen` 是一个经过高度优化的库。
        - **架构灵活性**: 如果未来需要替换 `Eigen`，只需修改类型别名定义处，而无需更改整个代码库。
    - **无坐标系信息**: 这些类型别名本身不包含任何坐标系信息，保持了数学核心的纯粹性。

#### 2. `math/transform/` (现有)

- **目的**: 提供纯粹的3D旋转变换计算。
- **文件**: `transform.hpp`, `types.hpp` 等。
- **设计要点**:
    - **`Transform` 类**: 核心类，内部使用四元数，对外提供欧拉角、旋转矩阵的接口。
    - **职责**: 负责“如何进行旋转计算”。
    - **扩展**: 需要确保 `Transform` 类可以与 `Tensor` 类（特别是 `Vector3`）进行运算，即重载 `Transform::operator*(const Vector3&)`。

---

### 第二层：坐标系统抽象层

**位置**: `include/gnc/coordination/`

这一层是连接纯数学与GNC应用的桥梁，定义了系统各部分交互的“契约”（接口）。

#### 1. `include/gnc/coordination/frame_identifier.hpp` (新)

- **目的**: 提供一个灵活、易于扩展的坐标系标识符方案。
- **设计要点**:
    - **`using FrameIdentifier = std::string;`**: 直接使用 `std::string` (或 `std::string_view` 以提高性能) 作为坐标系的唯一标识符。这给予了用户最大的灵活性，可以随时定义新的坐标系而无需修改核心代码。
    - **约定优于配置**: 框架不预设任何“核心”坐标系。用户对自己定义的字符串负责，常见的坐标系（如 `"BODY"`, `"INERTIAL"`）将作为一种事实标准存在于应用代码中。

#### 2. `include/gnc/coordination/itransform_provider.hpp` (新)

- **目的**: 定义“变换提供者”的抽象接口，这是解耦的关键。
- **设计要点**:
    - **`class ITransformProvider`**: 纯抽象基类。
    - **核心虚函数**:
        ```cpp
        virtual gnc::math::transform::Transform getTransform(
            const FrameIdentifier& from,
            const FrameIdentifier& to
        ) const = 0;
        ```
    - **职责**: 它的唯一职责是“回答任意两个坐标系之间的变换关系是什么”。它不关心这个变换是如何计算出来的。

#### 3. `include/gnc/coordination/frame_tensor.hpp` (新)

- **目的**: 创建一个坐标系感知的张量包装器，提供便捷的坐标转换API。
- **设计要点**:
    - **`template<typename TensorType>` class FrameTensor**: 模板类，可以包装任意类型的纯数学张量。
    - **成员变量**:
        - `TensorType m_tensor`: 存储的纯数学张量对象。
        - `FrameIdentifier m_frame`: 标记当前张量所在的坐标系。
        - `const ITransformProvider& m_provider`: **持有对抽象接口的引用**，用于查询变换。
    - **构造函数**: `FrameTensor(const TensorType&, const FrameIdentifier&, const ITransformProvider&)`，强制注入依赖。
    - **核心方法 `to(const FrameIdentifier& new_frame)`**:
        1. 检查 `new_frame` 是否与当前 `m_frame` 相同，若相同则直接返回。
        2. 调用 `m_provider.getTransform(m_frame, new_frame)` 获取变换矩阵 `T`。
        3. 使用 `T` 对 `m_tensor` 进行数学变换，得到 `new_tensor`。
        4. 返回一个新的 `FrameTensor(new_tensor, new_frame, m_provider)`。
    - **操作符重载**: 可以考虑重载 `+`, `-` 等，但必须检查操作数是否在同一坐标系下，如果不是，则抛出异常或进行自动转换（需谨慎设计）。

---

### 第三层：应用与实现层

**位置**: `src/coordination/`, `include/gnc/components/`

这一层包含与GNC项目具体逻辑相关的实现代码。

#### 1. `gnc/coordination/coordinate_system_registry.hpp` (新)

- **目的**: 管理坐标系之间的拓扑关系图。
- **设计要点**:
    - **`class CoordinateSystemRegistry`**:
    - **数据结构**: 使用图数据结构（如 `std::unordered_map<FrameIdentifier, std::vector<Edge>>`）来存储坐标系之间的直接变换关系。`Edge` 可以是一个包含目标 `FrameIdentifier` 和变换函数的结构体。
    - **注册接口**:
        - `addStaticTransform(const FrameIdentifier& from, const FrameIdentifier& to, const Transform& t)`: 注册一个固定不变的变换。
        - `addDynamicTransform(const FrameIdentifier& from, const FrameIdentifier& to, std::function<Transform()>)`: 注册一个动态变换。这里的 `lambda` 函数在被调用时，需要能访问到外部的状态（这是下一部分 `GncTransformProvider` 要解决的问题）。
    - **路径查找**:
        - `findPath(const FrameIdentifier& from, const FrameIdentifier& to)`: 实现一个图搜索算法（如BFS或Dijkstra），找到从 `from` 到 `to` 的变换路径。返回一个 `std::vector<Edge>`。

#### 2. `gnc/coordination/gnc_transform_provider.hpp` (新)

- **目的**: `ITransformProvider` 接口的具体实现，是整个系统中唯一一处“脏”代码，负责连接状态和变换。
- **设计要点**:
    - **`class GncTransformProvider : public ITransformProvider`**:
    - **成员变量**:
        - `StateManager& m_stateManager`: 持有对GNC状态管理器的引用。
        - `CoordinateSystemRegistry m_registry`: 持有坐标系注册表。
    - **构造函数**:
        - 接受 `StateManager` 的引用。
        - 在构造函数内部，初始化 `m_registry`，注册所有已知的静态和动态变换。
        - **关键点**: 注册动态变换时，传入的 `lambda` 函数会捕获 `this` 指针，从而可以在需要时通过 `this->m_stateManager` 获取状态。
            ```cpp
            // 示例
            m_registry.addDynamicTransform("INERTIAL", "BODY", [this]() {
                auto attitude = this->m_stateManager.getAttitude(); // 获取姿态
                return Transform::FromEuler(attitude);
            });
            ```
    - **`getTransform` 实现**:
        1. 调用 `m_registry.findPath(from, to)` 获取变换路径。
        2. 遍历路径，对每一步的变换进行组合。
        3. 如果某一步是动态变换，就调用其 `lambda` 函数获取当前的变换矩阵。
        4. 将所有变换组合起来，返回最终的 `Transform`。
        5.- **缓存**: 可以增加一个缓存机制（如 `std::map<std::pair<FrameIdentifier, FrameIdentifier>, Transform>`），缓存同一仿真步内已经计算过的变换，避免重复计算。在每个仿真步开始时清空缓存。

---

## 使用流程示例

```cpp
// 1. 在顶层应用初始化时
StateManager stateManager;
GncTransformProvider transformProvider(stateManager); // 创建唯一实例

// 2. 在组件内部或计算逻辑中
void someGncFunction(const ITransformProvider& provider) {
    // 创建一个在BODY系下的力向量
    gnc::math::Vector3d force_in_body_data(100.0, 0, 0);
    FrameTensor<gnc::math::Vector3d> force_b(force_in_body_data, "BODY", provider);

    // 将其转换到INERTIAL系
    FrameTensor<gnc::math::Vector3d> force_i = force_b.to("INERTIAL");

    // ... 使用 force_i.getTensor() 进行后续计算
}

// 3. 调用
someGncFunction(transformProvider);
```