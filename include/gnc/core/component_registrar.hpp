#pragma once

#include "component_factory.hpp"
#include <string>

namespace gnc {

template <typename T>
class ComponentRegistrar {
public:
    /**
     * @brief 构造函数会在main之前被调用，从而实现自注册
     * @param type 要注册的组件类型字符串
     */
    explicit ComponentRegistrar(const std::string& type) {
        // 获取工厂实例，并注册一个lambda函数作为创建者
        // 这个lambda函数知道如何创建类型为T的组件
        ComponentFactory::getInstance().registerCreator(type, 
            [](states::VehicleId id) -> states::ComponentBase* {
                return new T(id);
            }
        );
    }
};

}
