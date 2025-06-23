/**
 * @file types.hpp
 * @brief 新状态系统的核心类型定义
 * @details 本文件定义了状态系统的基础类型，包括状态标识、访问控制等核心概念。
 * 状态系统采用分布式设计，通过唯一标识符和类型安全机制确保状态管理的可靠性。
 */
#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <any>

namespace gnc::states {

/** 
 * @brief 飞行器标识符类型
 * @details 使用64位无符号整数作为飞行器的唯一标识，支持大规模分布式系统
 */
using VehicleId = uint64_t;   

/**
 * @brief 组件标识符
 * @details 组件标识符用于唯一标识一个组件，由两个部分组成：
 * - vehicleId: 标识组件所属的飞行器
 * - name: 组件的名称
 * 
 * 这种二级标识结构支持：
 * 1. 多飞行器并行控制
 * 2. 组件级别的唯一标识
 */
struct ComponentId {
    VehicleId vehicleId{0};      // 飞行器ID
    std::string name;         // 组件名称

    /** 
     * @brief 默认构造函数
     */
    ComponentId() = default;

    /**
     * @brief 构造函数
     */
    ComponentId(VehicleId id, std::string n)
        : vehicleId(id), name(std::move(n)) {}

    /** 
     * @brief 组件标识符相等性比较
     */
    bool operator==(const ComponentId& other) const {
        return vehicleId == other.vehicleId &&
               name == other.name;
    }
};

/**
 * @brief 状态标识符
 * @details 状态标识符是状态系统中的核心概念，用于唯一标识一个状态变量
 * 由三个部分组成：
 * - vehicleId: 标识状态所属的飞行器
 * - component: 标识状态所属的组件
 * - name: 状态的具体名称
 * 
 * 这种三级标识结构支持：
 * 1. 多飞行器并行控制
 * 2. 组件级别的状态隔离
 * 3. 状态级别的精确访问控制
 */
struct StateId {
    ComponentId component;   // 组件ID
    std::string name;        // 状态名称

    /** 
     * @brief 状态标识符相等性比较
     * @details 用于在状态容器中进行查找和比较操作
     */
    bool operator==(const StateId& other) const {
        return component == other.component &&
               name == other.name;
    }
};

/**
 * @brief 状态访问类型
 * @details 定义状态的访问权限，用于实现状态的访问控制
 * - Input: 表示组件需要读取该状态
 * - Output: 表示组件会写入该状态
 */
enum class StateAccessType {
    Input,   ///< 输入状态，组件要求该输入
    Output,  ///< 输出状态，组件会更新该状态
};

/**
 * @brief 状态规格定义
 * @details 描述状态的完整特征，包括：
 * - name: 状态名称
 * - type: 状态的数据类型
 * - access: 访问权限
 * - source: 输入状态的数据来源
 * - required: 是否为必需状态
 * 
 * 状态规格用于：
 * 1. 组件接口定义
 * 2. 状态依赖分析
 * 3. 系统完整性验证
 */
struct StateSpec {
    std::string name;        ///< 状态名称
    std::string type;        ///< 状态类型名称
    StateAccessType access;  ///< 访问权限
    std::optional<StateId> source;  ///< 数据来源（仅输入状态）
    bool required{false};    ///< 是否必需
    std::any default_value; ///< 默认值
};

} // namespace gnc::states

// 为标识符类型添加哈希支持
namespace std {
template<>
struct hash<gnc::states::ComponentId> {
    /** 
     * @brief ComponentId的哈希函数
     * @details 组合vehicleId和name的哈希值
     */
    size_t operator()(const gnc::states::ComponentId& id) const {
        return hash<gnc::states::VehicleId>()(id.vehicleId) ^
               hash<string>()(id.name);
    }
};

template<>
struct hash<gnc::states::StateId> {
    /** 
     * @brief StateId的哈希函数
     * @details 组合三个字段的哈希值，用于在无序容器中存储StateId
     */
    size_t operator()(const gnc::states::StateId& id) const {
        return hash<gnc::states::VehicleId>()(id.component.vehicleId) ^
               hash<string>()(id.component.name) ^
               hash<string>()(id.name);
    }
};
}