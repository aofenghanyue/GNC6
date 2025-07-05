/**
 * @file frame_identifier.hpp
 * @brief 坐标系标识符定义
 * 
 * 提供预定义的坐标系名称常量，避免字符串拼写错误
 */

#pragma once

#include <string>
#include <unordered_set>

namespace gnc {
namespace coordination {

// 使用字符串作为坐标系标识符
using FrameIdentifier = std::string;
using FrameIdentifierSet = std::unordered_set<FrameIdentifier>;

/**
 * @brief 预定义坐标系名称
 * 
 * 防止拼写错误的常量定义
 */
namespace frames {

// 惯性坐标系
constexpr const char* INERTIAL = "INERTIAL";

// 载体坐标系
constexpr const char* BODY = "BODY";

// 北东地坐标系
constexpr const char* NED = "NED";

// 东北天坐标系
constexpr const char* ENU = "ENU";

// 地心地固坐标系
constexpr const char* ECEF = "ECEF";

// 地心惯性坐标系
constexpr const char* ECI = "ECI";

// 风轴坐标系
constexpr const char* WIND = "WIND";

// 稳定轴坐标系
constexpr const char* STABILITY = "STABILITY";

// 地面坐标系
constexpr const char* GROUND = "GROUND";

// 发射台坐标系
constexpr const char* LAUNCH = "LAUNCH";

// 目标坐标系
constexpr const char* TARGET = "TARGET";

// 传感器坐标系
constexpr const char* SENSOR = "SENSOR";

// 相机坐标系
constexpr const char* CAMERA = "CAMERA";

// 激光雷达坐标系
constexpr const char* LIDAR = "LIDAR";

// GPS坐标系
constexpr const char* GPS = "GPS";

} // namespace frames

// 简化的工具函数
namespace validation {

// 简单的相等比较（忽略大小写）
inline bool areFramesEqual(const std::string& frame1, const std::string& frame2) {
    if (frame1.length() != frame2.length()) {
        return false;
    }
    
    for (size_t i = 0; i < frame1.length(); ++i) {
        if (std::toupper(frame1[i]) != std::toupper(frame2[i])) {
            return false;
        }
    }
    return true;
}

// 基本的有效性检查（非空即可）
inline bool isValidFrameId(const std::string& frame_id) {
    return !frame_id.empty();
}

} // namespace validation

} // namespace coordination
} // namespace gnc