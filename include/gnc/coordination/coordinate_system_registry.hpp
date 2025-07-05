/**
 * @file coordinate_system_registry.hpp
 * @brief 坐标系注册表
 * 
 * 本文件定义了坐标系注册表类，负责管理坐标系之间的拓扑关系图。
 * 支持静态变换和动态变换的注册，并提供路径查找算法。
 * 
 * 设计理念：
 * - 图结构：使用有向图表示坐标系间的变换关系
 * - 路径查找：实现BFS算法找到最短变换路径
 * - 灵活性：支持静态和动态变换的混合使用
 * - 可扩展性：易于添加新的坐标系和变换关系
 * 
 * 使用示例：
 * @code
 * CoordinateSystemRegistry registry;
 * 
 * // 注册静态变换
 * registry.addStaticTransform("BODY", "NED", body_to_ned_transform);
 * 
 * // 注册动态变换
 * registry.addDynamicTransform("NED", "ECEF", []() {
 *     return getCurrentNedToEcefTransform();
 * });
 * 
 * // 查找变换路径
 * auto path = registry.findTransformPath("BODY", "ECEF");
 * @endcode
 */

#pragma once

#include "frame_identifier.hpp"
#include "itransform_provider.hpp"
#include "../../math/math.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <functional>
#include <memory>
#include <optional>

namespace gnc {
namespace coordination {

/**
 * @brief 变换边的类型
 */
enum class TransformEdgeType {
    STATIC,  ///< 静态变换（固定不变）
    DYNAMIC  ///< 动态变换（依赖当前状态）
};

/**
 * @brief 变换边描述
 * 
 * 表示从一个坐标系到另一个坐标系的变换关系
 */
struct TransformEdge {
    FrameIdentifier from_frame;      ///< 源坐标系
    FrameIdentifier to_frame;        ///< 目标坐标系
    TransformEdgeType edge_type;     ///< 边的类型
    
    /// 静态变换数据
    Transform static_transform;
    
    /// 动态变换函数
    std::function<Transform()> dynamic_transform_func;
    
    std::string description;         ///< 变换描述

    /**
     * @brief 默认构造函数
     */
    TransformEdge() 
        : from_frame(""), to_frame(""), edge_type(TransformEdgeType::STATIC),
          static_transform(Transform::Identity()),
          dynamic_transform_func(nullptr), description("") {}

    /**
     * @brief 静态变换构造函数
     */
    TransformEdge(const FrameIdentifier& from, 
                 const FrameIdentifier& to,
                 const Transform& transform,
                 const std::string& desc = "")
        : from_frame(from), to_frame(to), edge_type(TransformEdgeType::STATIC),
          static_transform(transform), description(desc) {}

    /**
     * @brief 动态变换构造函数
     */
    TransformEdge(const FrameIdentifier& from,
                 const FrameIdentifier& to,
                 std::function<Transform()> func,
                 const std::string& desc = "")
        : from_frame(from), to_frame(to), edge_type(TransformEdgeType::DYNAMIC),
          static_transform(Transform::Identity()),
          dynamic_transform_func(std::move(func)), description(desc) {}

    /**
     * @brief 获取当前变换
     */
    Transform getTransform() const {
        if (edge_type == TransformEdgeType::STATIC) {
            return static_transform;
        } else {
            if (!dynamic_transform_func) {
                throw std::runtime_error("Dynamic transform function is null");
            }
            return dynamic_transform_func();
        }
    }

    /**
     * @brief 检查边是否有效
     */
    bool isValid() const {
        if (edge_type == TransformEdgeType::DYNAMIC) {
            return dynamic_transform_func != nullptr;
        }
        return true; // 静态变换总是有效的
    }
};

/**
 * @brief 变换路径
 * 
 * 表示从源坐标系到目标坐标系的完整变换路径
 */
struct TransformPath {
    FrameIdentifier from_frame;      ///< 源坐标系
    FrameIdentifier to_frame;        ///< 目标坐标系
    std::vector<TransformEdge> edges; ///< 变换边序列
    
    /**
     * @brief 计算路径的总变换
     */
    Transform computeTransform() const {
        auto result = Transform::Identity();
        
        for (const auto& edge : edges) {
            result = result * edge.getTransform();
        }
        
        return result;
    }

    /**
     * @brief 检查路径是否有效
     */
    bool isValid() const {
        if (edges.empty()) {
            return validation::areFramesEqual(from_frame, to_frame);
        }
        
        // 检查路径连续性
        if (!validation::areFramesEqual(edges.front().from_frame, from_frame) ||
            !validation::areFramesEqual(edges.back().to_frame, to_frame)) {
            return false;
        }
        
        for (size_t i = 1; i < edges.size(); ++i) {
            if (!validation::areFramesEqual(edges[i-1].to_frame, edges[i].from_frame)) {
                return false;
            }
        }
        
        // 检查所有边是否有效
        for (const auto& edge : edges) {
            if (!edge.isValid()) {
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief 获取路径描述
     */
    std::string getDescription() const {
        if (edges.empty()) {
            return "Identity transform";
        }
        
        std::string desc = from_frame;
        for (const auto& edge : edges) {
            desc += " -> " + edge.to_frame;
            if (!edge.description.empty()) {
                desc += " (" + edge.description + ")";
            }
        }
        return desc;
    }
};

/**
 * @brief 坐标系注册表类
 * 
 * 管理坐标系之间的拓扑关系图，支持静态和动态变换的注册和查找。
 * 
 * 主要功能：
 * - 注册静态和动态变换关系
 * - 查找坐标系间的变换路径
 * - 支持逆变换的自动处理
 * - 提供调试和诊断信息
 */
class CoordinateSystemRegistry {
public:
    /**
     * @brief 构造函数
     */
    CoordinateSystemRegistry() = default;

    /**
     * @brief 析构函数
     */
    ~CoordinateSystemRegistry() = default;

    // 禁止拷贝和移动
    CoordinateSystemRegistry(const CoordinateSystemRegistry&) = delete;
    CoordinateSystemRegistry& operator=(const CoordinateSystemRegistry&) = delete;
    CoordinateSystemRegistry(CoordinateSystemRegistry&&) = delete;
    CoordinateSystemRegistry& operator=(CoordinateSystemRegistry&&) = delete;

    // ==================== 变换注册接口 ====================

    /**
     * @brief 注册静态变换
     * 
     * 静态变换是固定不变的变换关系，不依赖当前状态。
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @param transform 变换矩阵
     * @param description 变换描述（可选）
     * @param bidirectional 是否同时注册逆变换
     * @return 是否注册成功
     */
    bool addStaticTransform(const FrameIdentifier& from_frame,
                           const FrameIdentifier& to_frame,
                           const Transform& transform,
                           const std::string& description = "",
                           bool bidirectional = true) {
        
        if (!validation::isValidFrameId(from_frame) || !validation::isValidFrameId(to_frame)) {
            return false;
        }

        // 添加正向变换
        auto edge = TransformEdge(from_frame, to_frame, transform, description);
        addEdge(edge);

        // 添加逆变换
        if (bidirectional && !validation::areFramesEqual(from_frame, to_frame)) {
            auto inverse_edge = TransformEdge(to_frame, from_frame, 
                                            transform.inverse(), 
                                            "Inverse of: " + description);
            addEdge(inverse_edge);
        }

        return true;
    }

    /**
     * @brief 注册动态变换
     * 
     * 动态变换依赖当前状态，每次调用时重新计算。
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @param transform_func 变换计算函数
     * @param description 变换描述（可选）
     * @param bidirectional 是否同时注册逆变换
     * @return 是否注册成功
     */
    bool addDynamicTransform(const FrameIdentifier& from_frame,
                            const FrameIdentifier& to_frame,
                            std::function<Transform()> transform_func,
                            const std::string& description = "",
                            bool bidirectional = true) {
        
        if (!validation::isValidFrameId(from_frame) || !validation::isValidFrameId(to_frame)) {
            return false;
        }

        if (!transform_func) {
            return false;
        }

        // 添加正向变换
        auto edge = TransformEdge(from_frame, to_frame, transform_func, description);
        addEdge(edge);

        // 添加逆变换
        if (bidirectional && !validation::areFramesEqual(from_frame, to_frame)) {
            auto inverse_func = [transform_func]() {
                return transform_func().inverse();
            };
            auto inverse_edge = TransformEdge(to_frame, from_frame, 
                                            inverse_func, 
                                            "Inverse of: " + description);
            addEdge(inverse_edge);
        }

        return true;
    }

    // ==================== 路径查找接口 ====================

    /**
     * @brief 查找变换路径
     * 
     * 使用BFS算法查找从源坐标系到目标坐标系的最短路径。
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @return 变换路径（如果找到）
     */
    std::optional<TransformPath> findTransformPath(const FrameIdentifier& from_frame,
                                                  const FrameIdentifier& to_frame) const {
        
        if (!validation::isValidFrameId(from_frame) || !validation::isValidFrameId(to_frame)) {
            return std::nullopt;
        }

        // 处理相同坐标系的情况
        if (validation::areFramesEqual(from_frame, to_frame)) {
            TransformPath path;
            path.from_frame = from_frame;
            path.to_frame = to_frame;
            // edges保持为空，表示单位变换
            return path;
        }

        // BFS搜索
        std::queue<FrameIdentifier> queue;
        std::unordered_set<FrameIdentifier> visited;
        std::unordered_map<FrameIdentifier, FrameIdentifier> parent;
        std::unordered_map<FrameIdentifier, TransformEdge> edge_map;

        queue.push(from_frame);
        visited.insert(from_frame);

        while (!queue.empty()) {
            auto current = queue.front();
            queue.pop();

            // 检查当前节点的所有邻居
            auto it = adjacency_list_.find(current);
            if (it != adjacency_list_.end()) {
                for (const auto& edge : it->second) {
                    if (visited.find(edge.to_frame) == visited.end()) {
                        visited.insert(edge.to_frame);
                        parent[edge.to_frame] = current;
                        edge_map[edge.to_frame] = edge;
                        queue.push(edge.to_frame);

                        // 找到目标
                        if (validation::areFramesEqual(edge.to_frame, to_frame)) {
                            return reconstructPath(from_frame, to_frame, parent, edge_map);
                        }
                    }
                }
            }
        }

        // 未找到路径
        return std::nullopt;
    }

    /**
     * @brief 检查是否存在变换路径
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @return 是否存在路径
     */
    bool hasTransformPath(const FrameIdentifier& from_frame,
                         const FrameIdentifier& to_frame) const {
        return findTransformPath(from_frame, to_frame).has_value();
    }

    // ==================== 查询接口 ====================

    /**
     * @brief 获取所有已注册的坐标系
     * 
     * @return 坐标系集合
     */
    FrameIdentifierSet getRegisteredFrames() const {
        FrameIdentifierSet frames;
        
        for (const auto& [frame, edges] : adjacency_list_) {
            frames.insert(frame);
            for (const auto& edge : edges) {
                frames.insert(edge.to_frame);
            }
        }
        
        return frames;
    }

    /**
     * @brief 获取指定坐标系的直接邻居
     * 
     * @param frame 坐标系标识符
     * @return 邻居坐标系集合
     */
    FrameIdentifierSet getDirectNeighbors(const FrameIdentifier& frame) const {
        FrameIdentifierSet neighbors;
        
        auto it = adjacency_list_.find(frame);
        if (it != adjacency_list_.end()) {
            for (const auto& edge : it->second) {
                neighbors.insert(edge.to_frame);
            }
        }
        
        return neighbors;
    }

    /**
     * @brief 获取变换边的数量
     * 
     * @return 边的总数
     */
    size_t getEdgeCount() const {
        size_t count = 0;
        for (const auto& [frame, edges] : adjacency_list_) {
            count += edges.size();
        }
        return count;
    }

    /**
     * @brief 获取坐标系的数量
     * 
     * @return 坐标系总数
     */
    size_t getFrameCount() const {
        return getRegisteredFrames().size();
    }

    // ==================== 调试和诊断接口 ====================

    /**
     * @brief 生成图的文本表示
     * 
     * @return 图的文本描述
     */
    std::string generateGraphDescription() const {
        std::string result = "Coordinate System Graph:\n";
        result += "========================\n";
        result += "Frames: " + std::to_string(getFrameCount()) + "\n";
        result += "Edges: " + std::to_string(getEdgeCount()) + "\n\n";

        for (const auto& [frame, edges] : adjacency_list_) {
            result += frame + ":\n";
            for (const auto& edge : edges) {
                result += "  -> " + edge.to_frame;
                std::string edge_type_string = edge.edge_type == TransformEdgeType::STATIC ? "Static" : "Dynamic";
                result += " (" + edge_type_string + ")";
                if (!edge.description.empty()) {
                    result += " [" + edge.description + "]";
                }
                result += "\n";
            }
            result += "\n";
        }

        return result;
    }

    /**
     * @brief 验证图的完整性
     * 
     * @return 验证结果和错误信息
     */
    std::pair<bool, std::string> validateGraph() const {
        std::vector<std::string> errors;

        // 检查所有边是否有效
        for (const auto& [frame, edges] : adjacency_list_) {
            for (const auto& edge : edges) {
                if (!edge.isValid()) {
                    errors.push_back("Invalid edge from " + edge.from_frame + 
                                   " to " + edge.to_frame);
                }
            }
        }

        bool is_valid = errors.empty();
        std::string error_message;
        if (!is_valid) {
            error_message = "Graph validation errors:\n";
            for (const auto& error : errors) {
                error_message += "- " + error + "\n";
            }
        }

        return {is_valid, error_message};
    }

    /**
     * @brief 清除所有注册的变换
     */
    void clear() {
        adjacency_list_.clear();
    }

private:
    /// 邻接表表示的有向图
    std::unordered_map<FrameIdentifier, std::vector<TransformEdge>> adjacency_list_;

    /**
     * @brief 添加边到图中
     * 
     * @param edge 要添加的边
     */
    void addEdge(const TransformEdge& edge) {
        adjacency_list_[edge.from_frame].push_back(edge);
    }

    /**
     * @brief 重构路径
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @param parent 父节点映射
     * @param edge_map 边映射
     * @return 重构的路径
     */
    TransformPath reconstructPath(
        const FrameIdentifier& from_frame,
        const FrameIdentifier& to_frame,
        const std::unordered_map<FrameIdentifier, FrameIdentifier>& parent,
        const std::unordered_map<FrameIdentifier, TransformEdge>& edge_map) const {
        
        TransformPath path;
        path.from_frame = from_frame;
        path.to_frame = to_frame;

        // 从目标向源回溯
        std::vector<TransformEdge> reversed_edges;
        FrameIdentifier current = to_frame;

        while (parent.find(current) != parent.end()) {
            reversed_edges.push_back(edge_map.at(current));
            current = parent.at(current);
        }

        // 反转边序列
        path.edges.reserve(reversed_edges.size());
        for (auto it = reversed_edges.rbegin(); it != reversed_edges.rend(); ++it) {
            path.edges.push_back(*it);
        }

        return path;
    }
};

} // namespace coordination
} // namespace gnc