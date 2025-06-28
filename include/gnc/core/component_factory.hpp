#pragma once

#include "component_base.hpp"
#include "../common/exceptions.hpp"
#include "../components/utility/simple_logger.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace gnc {

// 创建者函数的类型定义：接受一个VehicleId，返回一个ComponentBase指针
using ComponentCreator = std::function<states::ComponentBase*(states::VehicleId)>;

class ComponentFactory {
public:
    static ComponentFactory& getInstance() {
        static ComponentFactory instance;
        return instance;
    }

    // 禁止拷贝和移动
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

    /**
     * @brief 注册一个组件创建者
     * @param type 组件的类型字符串
     * @param creator 创建该组件的函数
     */
    void registerCreator(const std::string& type, ComponentCreator creator) {
        if (creators_.find(type) != creators_.end()) {
            // 可以根据需要决定是抛出异常还是仅记录警告
            LOG_WARN("[Factory] Component type '{}' is already registered. Overwriting.", type.c_str());
        }
        creators_[type] = std::move(creator);
        LOG_DEBUG("[Factory] Registered component type: {}", type.c_str());
    }

    /**
     * @brief 根据类型字符串创建组件实例
     * @param type 要创建的组件类型
     * @param id 飞行器ID
     * @return 指向新创建组件的指针，如果类型未注册则返回nullptr
     */
    states::ComponentBase* createComponent(const std::string& type, states::VehicleId id) {
        auto it = creators_.find(type);
        if (it == creators_.end()) {
            LOG_ERROR("[Factory] Component type '{}' not found.", type.c_str());
            throw gnc::ConfigurationError("ComponentFactory", "Component type '" + type + "' not registered.");
        }
        return it->second(id);
    }

private:
    ComponentFactory() = default;
    ~ComponentFactory() = default;

    std::unordered_map<std::string, ComponentCreator> creators_;
};

}
