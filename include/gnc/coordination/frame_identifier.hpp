/**
 * @file frame_identifier.hpp
 * @brief 坐标系标识符定义
 * 
 * 本文件定义了坐标系的标识符类型和相关工具。
 * 使用字符串作为坐标系的唯一标识符，提供最大的灵活性。
 * 
 * 设计理念：
 * - 简单灵活：使用字符串作为标识符，用户可以自由定义
 * - 约定优于配置：不预设"核心"坐标系，依赖用户约定
 * - 类型安全：提供类型别名和验证函数
 * 
 * 常用坐标系约定：
 * - "INERTIAL": 惯性坐标系
 * - "BODY": 载体坐标系
 * - "NED": 北东地坐标系
 * - "ECEF": 地心地固坐标系
 * - "ECI": 地心惯性坐标系
 * 
 * 使用示例：
 * @code
 * #include <gnc/coordination/frame_identifier.hpp>
 * using namespace gnc::coordination;
 * 
 * FrameIdentifier body_frame = "BODY";
 * FrameIdentifier inertial_frame = "INERTIAL";
 * 
 * if (isValidFrameId(body_frame)) {
 *     // 使用坐标系标识符
 * }
 * @endcode
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <algorithm>
#include <cctype>

namespace gnc {
namespace coordination {

/**
 * @brief 坐标系标识符类型
 * 
 * 使用字符串作为坐标系的唯一标识符。这种设计提供了最大的灵活性，
 * 允许用户根据需要定义任意的坐标系名称。
 * 
 * 推荐命名约定：
 * - 使用大写字母和下划线
 * - 简洁明了，避免歧义
 * - 遵循航空航天领域的标准命名
 */
using FrameIdentifier = std::string;

/**
 * @brief 坐标系标识符视图类型
 * 
 * 用于函数参数，避免不必要的字符串拷贝
 */
using FrameIdentifierView = std::string_view;

/**
 * @brief 坐标系标识符集合类型
 */
using FrameIdentifierSet = std::unordered_set<FrameIdentifier>;

/**
 * @brief 常用坐标系标识符
 * 
 * 提供一些航空航天领域常用的坐标系标识符常量
 */
namespace frames {

/// 惯性坐标系（Inertial Frame）
constexpr const char* INERTIAL = "INERTIAL";

/// 载体坐标系（Body Frame）
constexpr const char* BODY = "BODY";

/// 北东地坐标系（North-East-Down）
constexpr const char* NED = "NED";

/// 东北天坐标系（East-North-Up）
constexpr const char* ENU = "ENU";

/// 地心地固坐标系（Earth-Centered Earth-Fixed）
constexpr const char* ECEF = "ECEF";

/// 地心惯性坐标系（Earth-Centered Inertial）
constexpr const char* ECI = "ECI";

/// 风轴坐标系（Wind Frame）
constexpr const char* WIND = "WIND";

/// 稳定轴坐标系（Stability Frame）
constexpr const char* STABILITY = "STABILITY";

/// 地面坐标系（Ground Frame）
constexpr const char* GROUND = "GROUND";

/// 发射台坐标系（Launch Frame）
constexpr const char* LAUNCH = "LAUNCH";

/// 目标坐标系（Target Frame）
constexpr const char* TARGET = "TARGET";

/// 传感器坐标系（Sensor Frame）
constexpr const char* SENSOR = "SENSOR";

/// 相机坐标系（Camera Frame）
constexpr const char* CAMERA = "CAMERA";

/// 激光雷达坐标系（LiDAR Frame）
constexpr const char* LIDAR = "LIDAR";

/// GPS坐标系（GPS Frame）
constexpr const char* GPS = "GPS";

} // namespace frames

/**
 * @brief 坐标系标识符验证工具
 */
namespace validation {

/**
 * @brief 检查坐标系标识符是否有效
 * 
 * 有效的标识符应该：
 * - 非空
 * - 不包含空白字符
 * - 长度合理（1-64字符）
 * 
 * @param frame_id 要检查的坐标系标识符
 * @return 是否有效
 */
inline bool isValidFrameId(FrameIdentifierView frame_id) {
    // 检查是否为空
    if (frame_id.empty()) {
        return false;
    }
    
    // 检查长度
    if (frame_id.length() > 64) {
        return false;
    }
    
    // 检查是否包含空白字符
    return std::none_of(frame_id.begin(), frame_id.end(), 
                       [](char c) { return std::isspace(c); });
}

/**
 * @brief 规范化坐标系标识符
 * 
 * 将标识符转换为大写并移除前后空白
 * 
 * @param frame_id 原始标识符
 * @return 规范化后的标识符
 */
inline FrameIdentifier normalizeFrameId(FrameIdentifierView frame_id) {
    std::string normalized;
    normalized.reserve(frame_id.length());
    
    // 复制并转换为大写，跳过空白字符
    for (char c : frame_id) {
        if (!std::isspace(c)) {
            normalized.push_back(std::toupper(c));
        }
    }
    
    return normalized;
}

/**
 * @brief 检查两个坐标系标识符是否相等（忽略大小写）
 * 
 * @param frame1 第一个坐标系标识符
 * @param frame2 第二个坐标系标识符
 * @return 是否相等
 */
inline bool areFramesEqual(FrameIdentifierView frame1, FrameIdentifierView frame2) {
    if (frame1.length() != frame2.length()) {
        return false;
    }
    
    return std::equal(frame1.begin(), frame1.end(), frame2.begin(),
                     [](char a, char b) {
                         return std::toupper(a) == std::toupper(b);
                     });
}

/**
 * @brief 创建有效的坐标系标识符
 * 
 * 如果输入无效，返回默认标识符
 * 
 * @param frame_id 输入标识符
 * @param default_frame 默认标识符
 * @return 有效的坐标系标识符
 */
inline FrameIdentifier createValidFrameId(FrameIdentifierView frame_id, 
                                         FrameIdentifierView default_frame = frames::INERTIAL) {
    auto normalized = normalizeFrameId(frame_id);
    if (isValidFrameId(normalized)) {
        return normalized;
    } else {
        return FrameIdentifier(default_frame);
    }
}

} // namespace validation

/**
 * @brief 坐标系标识符工具函数
 */
namespace utils {

/**
 * @brief 从字符串集合创建坐标系标识符集合
 * 
 * @param frame_names 坐标系名称集合
 * @return 坐标系标识符集合
 */
inline FrameIdentifierSet createFrameSet(const std::initializer_list<const char*>& frame_names) {
    FrameIdentifierSet frame_set;
    for (const auto& name : frame_names) {
        if (validation::isValidFrameId(name)) {
            frame_set.insert(validation::normalizeFrameId(name));
        }
    }
    return frame_set;
}

/**
 * @brief 检查坐标系是否在给定集合中
 * 
 * @param frame_id 要检查的坐标系标识符
 * @param frame_set 坐标系集合
 * @return 是否包含
 */
inline bool isFrameInSet(FrameIdentifierView frame_id, const FrameIdentifierSet& frame_set) {
    auto normalized = validation::normalizeFrameId(frame_id);
    return frame_set.find(normalized) != frame_set.end();
}

/**
 * @brief 获取常用坐标系集合
 * 
 * @return 包含常用坐标系的集合
 */
inline FrameIdentifierSet getCommonFrames() {
    return createFrameSet({
        frames::INERTIAL, frames::BODY, frames::NED, frames::ENU,
        frames::ECEF, frames::ECI, frames::WIND, frames::STABILITY,
        frames::GROUND, frames::LAUNCH, frames::TARGET
    });
}

} // namespace utils

} // namespace coordination
} // namespace gnc