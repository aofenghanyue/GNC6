// 已经摒弃了这种中间件，改用disturb组件统一管理拉偏
#pragma once
#include "../../core/component_base.hpp"
#include <vector>
#include "simple_logger.hpp"

namespace gnc::components {

// 演示如何实现一个通用的参数拉偏适配器
class BiasAdapter : public states::ComponentBase {
public:
    BiasAdapter(states::VehicleId id, const std::string& name, const states::ComponentId& input_component, const std::string& output_name, double bias_factor)
        : states::ComponentBase(id, name), bias_factor_(bias_factor), output_name_(output_name) {
        
        // 简化的组件级依赖声明
        declareInput<void>(input_component);
        declareOutput<std::vector<double>>(output_name);
    }

    std::string getComponentType() const override {
        return "BiasAdapter";
    }
protected:
    void updateImpl() override {
        // Note: This component is deprecated. The implementation would need to be updated
        // to work with the new simplified dependency system where specific state access
        // would need to be done through the get() method with explicit paths.
        
        // Example placeholder implementation:
        std::vector<double> biased_value = {1.0, 2.0, 3.0}; // Placeholder
        
        // 输出拉偏后的值
        setState(output_name_, biased_value);
        LOG_COMPONENT_DEBUG("BiasAdapter (deprecated) applied bias factor {}", bias_factor_);
    }
private:
    double bias_factor_;
    std::string output_name_;
};

}