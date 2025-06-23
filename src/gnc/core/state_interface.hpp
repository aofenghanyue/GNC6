
/**
 * @file state_interface.hpp
 * @brief 状态接口声明
 * @details 定义组件的状态接口，包括输入和输出状态的声明。
 * 本模块实现了组件间的状态依赖管理和接口验证机制。
 * 
 * 设计思路：
 * 1. 显式声明：组件必须显式声明其依赖的输入状态和提供的输出状态
 * 2. 类型安全：在编译时和运行时都进行类型检查
 * 3. 依赖管理：支持可选和必需依赖的区分
 * 4. 接口验证：提供完整性检查机制
 */
#pragma once
#include "../common/types.hpp"
#include "../common/exceptions.hpp"
#include <vector>
#include <optional>
#include <algorithm>
#include <string>

namespace gnc::states {

/**
 * @brief 状态接口类
 * @details 用于声明组件的输入和输出状态，管理组件间的依赖关系
 * 
 * 使用示例：
 * @code
 * StateInterface interface;
 * 
 * // 声明输入状态
 * interface.addInput({
 *     .name = "position",
 *     .type = "Tensor1",
 *     .required = true,
 *     .source = sourceStateId,
 *     .required = true
 * });
 * 
 * // 声明输出状态
 * interface.addOutput({
 *     .name = "velocity",
 *     .type = "Tensor1"
 * });
 * 
 * // 验证接口完整性
 * interface.validate();
 * @endcode
 */
class StateInterface {
public:
    /**
     * @brief 添加输入状态
     * @param spec 状态规格
     * @throw ValidationError 规格验证失败时抛出
     */
    void addInput(const StateSpec& spec) {
        validateSpec(spec, StateAccessType::Input);
        inputs_.push_back(spec);
    }

    /**
     * @brief 添加输出状态
     * @param spec 状态规格
     * @throw ValidationError 规格验证失败时抛出
     */
    void addOutput(const StateSpec& spec) {
        validateSpec(spec, StateAccessType::Output);
        outputs_.push_back(spec);
    }

    /**
     * @brief 查找输入状态
     * @param name 状态名称
     * @return 状态规格的可选引用
     */
    std::optional<StateSpec> findInput(const std::string& name) const {
        return findSpec(inputs_, name);
    }

    /**
     * @brief 查找输出状态
     * @param name 状态名称
     * @return 状态规格的可选引用
     */
    std::optional<StateSpec> findOutput(const std::string& name) const {
        return findSpec(outputs_, name);
    }

    const std::vector<StateSpec>& getInputs() const { return inputs_; }
    const std::vector<StateSpec>& getOutputs() const { return outputs_; }

    /**
     * @brief 验证接口完整性
     * @details 检查：
     * 1. 必需输入状态是否有源
     * 2. 输入源是否有效
     * 3. 状态名称是否唯一
     * @throw ValidationError 验证失败时抛出
     */
    void validate() const {
        // 检查必需的输入状态是否有源
        for (const auto& spec : inputs_) {
            if (spec.required && !spec.source) {
                throw ValidationError(
                    "State Interface",
                    "Required input '" + spec.name + "' has no source"
                );
            }
        }

        // 检查输入源是否有效
        for (const auto& spec : inputs_) {
            if (spec.source) {
                if (spec.source->component.name.empty()) {
                    throw ValidationError(
                        "State Interface",
                        "Input '" + spec.name + "' has invalid source component name"
                    );
                }
                if (spec.source->name.empty()) {
                    throw ValidationError(
                        "State Interface",
                        "Input '" + spec.name + "' has invalid source state name"
                    );
                }
            }
        }
    }

private:
    std::vector<StateSpec> inputs_;
    std::vector<StateSpec> outputs_;

    /**
     * @brief 在状态列表中查找指定名称的状态
     * @param specs 状态列表
     * @param name 状态名称
     * @return 找到的状态规格
     */
    static std::optional<StateSpec> findSpec(
        const std::vector<StateSpec>& specs, 
        const std::string& name) 
    {
        auto it = std::find_if(specs.begin(), specs.end(),
            [&name](const StateSpec& spec) {
                return spec.name == name;
            });
        if (it != specs.end()) {
            return *it;
        }
        return std::nullopt;
    }

    void validateSpec(const StateSpec& spec, StateAccessType expectedAccess) {
        if (spec.name.empty()) {
            throw ValidationError(
                "State Interface",
                "State name cannot be empty"
            );
        }
        if (spec.type.empty()) {
            throw ValidationError(
                "State Interface",
                "Type cannot be empty for state: " + spec.name
            );
        }
        if (spec.access != expectedAccess) {
            throw ValidationError(
                "State Interface",
                "Invalid access type for state: " + spec.name
            );
        }
        if (findInput(spec.name) || findOutput(spec.name)) {
            throw ValidationError(
                "State Interface",
                "Duplicate state name: " + spec.name
            );
        }
    }
};

} // namespace gnc::states