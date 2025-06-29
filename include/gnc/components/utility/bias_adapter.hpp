// 已经摒弃了这种中间件，改用disturb组件统一管理拉偏
#pragma once
#include "../../core/component_base.hpp"
#include <vector>
#include "simple_logger.hpp"

namespace gnc::components {

// 演示如何实现一个通用的参数拉偏适配器
class BiasAdapter : public states::ComponentBase {
public:
    BiasAdapter(states::VehicleId id, const std::string& name, const states::StateId& input_source, const std::string& output_name, double bias_factor)
        : states::ComponentBase(id, name), bias_factor_(bias_factor) {
        
        // 输入和输出类型必须匹配，这里用vector<double>举例
        declareInput<std::vector<double>>(input_source.name, input_source);
        declareOutput<std::vector<double>>(output_name);
    }

    std::string getComponentType() const override {
        return "BiasAdapter";
    }
protected:
    void updateImpl() override {
        // 从上游组件获取原始值
        auto original_value = getState<std::vector<double>>(getInterface().getInputs()[0].source.value());

        // 应用拉偏
        std::vector<double> biased_value = original_value;
        for(auto& val : biased_value) {
            val *= bias_factor_;
        }
        
        // 输出拉偏后的值
        setState(getInterface().getOutputs()[0].name, biased_value);
        LOG_COMPONENT_DEBUG("Applied bias factor {}. Original: {}, Biased: {}", 
                  bias_factor_, original_value[0], biased_value[0]);
    }
private:
    double bias_factor_;
};

}