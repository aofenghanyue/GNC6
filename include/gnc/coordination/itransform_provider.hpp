/**
 * @file itransform_provider.hpp
 * @brief 变换提供者抽象接口
 * 
 * 本文件定义了变换提供者的抽象接口，这是坐标转换系统解耦的关键。
 * 通过依赖倒置原则，上层组件依赖于此抽象接口，而不是具体实现。
 * 
 * 设计理念：
 * - 单一职责：只负责提供坐标系间的变换关系
 * - 接口分离：只暴露必要的方法
 * - 依赖倒置：上层依赖抽象，不依赖具体实现
 * - 可测试性：便于单元测试和模拟
 * 
 * 使用示例：
 * @code
 * #include <gnc/coordination/itransform_provider.hpp>
 * 
 * void processData(const ITransformProvider& provider) {
 *     auto transform = provider.getTransform("BODY", "INERTIAL");
 *     // 使用变换处理数据
 * }
 * @endcode
 */

#pragma once

#include "frame_identifier.hpp"
#include "../../math/transform/transform.hpp"
#include <stdexcept>

namespace gnc {
namespace coordination {

/**
 * @brief 变换查询异常
 * 
 * 当无法找到请求的坐标系变换时抛出此异常
 */
class TransformNotFoundError : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @param additional_info 额外信息
     */
    TransformNotFoundError(const FrameIdentifier& from_frame, 
                          const FrameIdentifier& to_frame,
                          const std::string& additional_info = "")
        : std::runtime_error(createMessage(from_frame, to_frame, additional_info)),
          from_frame_(from_frame), to_frame_(to_frame) {}

    /**
     * @brief 获取源坐标系
     */
    const FrameIdentifier& getFromFrame() const { return from_frame_; }

    /**
     * @brief 获取目标坐标系
     */
    const FrameIdentifier& getToFrame() const { return to_frame_; }

private:
    FrameIdentifier from_frame_;
    FrameIdentifier to_frame_;

    static std::string createMessage(const FrameIdentifier& from_frame,
                                   const FrameIdentifier& to_frame,
                                   const std::string& additional_info) {
        std::string message = "Transform not found from '" + from_frame + 
                             "' to '" + to_frame + "'";
        if (!additional_info.empty()) {
            message += ": " + additional_info;
        }
        return message;
    }
};

/**
 * @brief 变换提供者抽象接口
 * 
 * 这个接口定义了获取坐标系变换的契约。具体实现可以从配置文件、
 * 状态管理器或其他数据源获取变换信息。
 * 
 * 职责：
 * - 提供任意两个坐标系之间的变换关系
 * - 处理变换链的自动计算
 * - 缓存机制（可选，由具体实现决定）
 * - 错误处理和异常抛出
 */
class ITransformProvider {
public:
    virtual ~ITransformProvider() = default;

    /**
     * @brief 获取从源坐标系到目标坐标系的变换
     * 
     * 这是接口的核心方法，负责返回两个坐标系之间的变换关系。
     * 实现者需要处理：
     * - 直接变换的查找
     * - 变换链的计算（A->B, B->C => A->C）
     * - 逆变换的计算（A->B => B->A）
     * - 缓存机制（可选）
     * 
     * @param from_frame 源坐标系标识符
     * @param to_frame 目标坐标系标识符
     * @return 变换矩阵（从源坐标系到目标坐标系）
     * @throws TransformNotFoundError 当无法找到变换时
     * 
     * @note 如果from_frame == to_frame，应返回单位变换
     * @note 实现应该是线程安全的（如果需要多线程支持）
     */
    virtual gnc::math::transform::Transform getTransform(
        const FrameIdentifier& from_frame,
        const FrameIdentifier& to_frame) const = 0;

    /**
     * @brief 检查是否存在从源坐标系到目标坐标系的变换
     * 
     * 这个方法允许在不抛出异常的情况下检查变换是否存在。
     * 默认实现尝试调用getTransform并捕获异常。
     * 
     * @param from_frame 源坐标系标识符
     * @param to_frame 目标坐标系标识符
     * @return 是否存在变换
     */
    virtual bool hasTransform(const FrameIdentifier& from_frame,
                             const FrameIdentifier& to_frame) const {
        try {
            getTransform(from_frame, to_frame);
            return true;
        } catch (const TransformNotFoundError&) {
            return false;
        }
    }

    /**
     * @brief 获取支持的坐标系列表
     * 
     * 返回此提供者支持的所有坐标系标识符。
     * 默认实现返回空集合，具体实现可以重写此方法。
     * 
     * @return 支持的坐标系集合
     */
    virtual FrameIdentifierSet getSupportedFrames() const {
        return FrameIdentifierSet{};
    }

    /**
     * @brief 检查指定坐标系是否受支持
     * 
     * @param frame_id 坐标系标识符
     * @return 是否受支持
     */
    virtual bool isFrameSupported(const FrameIdentifier& frame_id) const {
        auto supported_frames = getSupportedFrames();
        return supported_frames.empty() || // 如果没有限制，认为都支持
               supported_frames.find(frame_id) != supported_frames.end();
    }

    /**
     * @brief 清除缓存（如果有的话）
     * 
     * 在仿真步骤开始时可能需要清除变换缓存，
     * 以确保使用最新的状态数据。
     * 默认实现为空，具体实现可以重写此方法。
     */
    virtual void clearCache() const {
        // 默认实现：什么都不做
    }

    /**
     * @brief 获取提供者信息
     * 
     * 返回提供者的描述信息，用于调试和日志记录。
     * 
     * @return 提供者描述字符串
     */
    virtual std::string getProviderInfo() const {
        return "Generic Transform Provider";
    }

protected:
    /**
     * @brief 验证坐标系标识符
     * 
     * 实现类可以使用此方法验证输入的坐标系标识符。
     * 
     * @param frame_id 要验证的坐标系标识符
     * @param parameter_name 参数名称（用于错误消息）
     * @throws std::invalid_argument 当标识符无效时
     */
    void validateFrameId(const FrameIdentifier& frame_id, 
                        const std::string& parameter_name = "frame_id") const {
        if (!validation::isValidFrameId(frame_id)) {
            throw std::invalid_argument("Invalid " + parameter_name + ": '" + frame_id + "'");
        }
    }

    /**
     * @brief 处理相同坐标系的情况
     * 
     * 当源坐标系和目标坐标系相同时，返回单位变换。
     * 
     * @param from_frame 源坐标系
     * @param to_frame 目标坐标系
     * @return 如果相同返回单位变换，否则返回空的optional
     */
    std::optional<gnc::math::transform::Transform> handleIdentityTransform(
        const FrameIdentifier& from_frame,
        const FrameIdentifier& to_frame) const {
        
        if (validation::areFramesEqual(from_frame, to_frame)) {
            return gnc::math::transform::Transform::Identity();
        }
        return std::nullopt;
    }
};

/**
 * @brief 变换提供者智能指针类型
 */
using ITransformProviderPtr = std::shared_ptr<ITransformProvider>;

/**
 * @brief 变换提供者常量智能指针类型
 */
using ITransformProviderConstPtr = std::shared_ptr<const ITransformProvider>;

} // namespace coordination
} // namespace gnc