/**
 * @file state_access.hpp
 * @brief 状态访问接口定义
 * @details 本文件定义了状态系统的核心访问接口，实现了类型安全的状态读写机制。
 * 采用模板方法模式，将类型安全检查和具体存储实现分离。
 * 
 * 设计思路：
 * 1. 类型安全：使用std::any和RTTI实现编译期和运行期的类型检查
 * 2. 接口分离：通过纯虚函数分离接口和实现
 * 3. 异常安全：确保类型转换失败时抛出适当的异常
 */
#pragma once
#include "../common/types.hpp"
#include <string>
#include <any>

namespace gnc::states {

/**
 * @brief 状态访问接口
 * @details 定义了状态系统的核心访问接口，提供类型安全的状态读写操作
 * 
 * 使用示例：
 * @code
 * class MyStateManager : public IStateAccess {
 *     // 实现具体的存储逻辑
 * };
 * 
 * MyStateManager manager;
 * manager.setState<double>(stateId, 3.14);
 * double value = manager.getState<double>(stateId);
 * @endcode
 */
class IStateAccess {
public:
    virtual ~IStateAccess() = default;

    template<typename T>
    const T& getState(const StateId& id) const {
        return *std::any_cast<const T>(&getStateImpl(id, typeid(T).name()));
    }

    /**
     * @brief 设置状态值
     * @tparam T 状态值类型
     * @param id 状态标识符
     * @param value 要设置的值
     * @throw StateTypeMismatchError 类型不匹配时抛出
     * @throw StateAccessError 访问权限错误时抛出
     */
    template<typename T>
    void setState(const StateId& id, const T& value) {
        setStateImpl(id, std::any(value), typeid(T).name());
    }

protected:
    /**
     * @brief 获取状态值的底层实现
     * @param id 状态标识符
     * @param type 类型名称（由RTTI生成）
     * @return 状态值的any封装
     */
    virtual const std::any& getStateImpl(const StateId& id, const std::string& type) const = 0;

    /**
     * @brief 设置状态值的底层实现
     * @param id 状态标识符
     * @param value 状态值的any封装
     * @param type 类型名称（由RTTI生成）
     */
    virtual void setStateImpl(const StateId& id, const std::any& value, const std::string& type) = 0;
};

} // namespace gnc::states