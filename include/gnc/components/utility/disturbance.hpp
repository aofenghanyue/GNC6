/**
 * @file disturbance_component.hpp
 * @brief 拉偏参数管理组件
 * 
 * @details 设计理念：
 * 1. 作为标准组件参与数据流，通过状态系统提供拉偏参数
 * 2. 支持静态参数（从配置文件加载）和动态参数（运行时计算）
 * 3. 支持单组参数（YAML）和多组参数（CSV）两种配置模式
 * 4. 其他组件通过标准的状态访问方法获取拉偏参数
 * 
 * @usage 使用示例：
 * @code
 * // 在其他组件中声明对拉偏参数的依赖
 * class IMUSensor : public ComponentBase {
 * public:
 *     IMUSensor(VehicleId id) : ComponentBase(id, "IMU") {
 *         // 简化的组件级依赖声明
 *         declareInput<void>(ComponentId{id, "Disturbance"});
 *         declareOutput<Vector3d>("angular_rate");
 *     }
 *     
 *     void initialize() override {
 *         gyro_bias_ = get<Vector3d>("gyro_bias");
 *     }
 *     
 *     void updateImpl() override {
 *         auto measured = true_rate + gyro_bias_;
 *         set("angular_rate", measured);
 *     }
 * };
 * @endcode
 */
#pragma once

#include "gnc/core/component_base.hpp"
#include "gnc/common/types.hpp"
#include "config_manager.hpp"
#include "gnc/core/component_registrar.hpp"
#include <map>
#include <vector>
#include <fstream>
#include <sstream>

namespace gnc::components {

/**
 * @brief 拉偏参数管理组件
 * 
 * @details 功能：
 * 1. 集中管理所有组件的拉偏参数
 * 2. 支持静态参数和简单的动态参数
 * 3. 支持单组参数和多组参数批量实验
 * 4. 通过状态系统向其他组件提供参数
 */
class Disturbance : public gnc::states::ComponentBase {
public:
    /**
     * @brief 构造函数
     * @param vehicleId 飞行器ID
     * @param instance_name 组件实例名（可选）
     */
    explicit Disturbance(gnc::states::VehicleId vehicleId, const std::string& instance_name = "")
        : ComponentBase(vehicleId, "Disturbance", instance_name) {
        // 预声明常用的拉偏参数输出
        declareCommonOutputs();
    }

    /**
     * @brief 获取组件类型名称
     */
    std::string getComponentType() const override {
        return "Disturb";
    }

    /**
     * @brief 初始化组件，加载拉偏参数配置
     */
    void initialize() override;

    /**
     * @brief 更新动态拉偏参数
     */
    void updateImpl() override;

    /**
     * @brief 切换参数集（用于批量实验）
     * @param set_index 参数集索引
     */
    void selectParameterSet(size_t set_index);

    /**
     * @brief 获取参数集总数
     * @return 参数集数量
     */
    size_t getParameterSetCount() const {
        return param_sets_.size();
    }

    /**
     * @brief 检查是否处于多组参数模式
     * @return true 如果支持多组参数
     */
    bool isMultiSetMode() const {
        return !param_sets_.empty();
    }

private:
    /**
     * @brief 预声明常用的拉偏参数输出
     */
    void declareCommonOutputs();

    /**
     * @brief 从YAML配置加载单组参数
     * @param params YAML参数节点
     */
    void loadSingleParameters(const nlohmann::json& params);

    /**
     * @brief 从CSV文件加载多组参数
     * @param filename CSV文件路径
     * @param active_set 当前激活的参数集索引
     */
    void loadCSVParameters(const std::string& filename, size_t active_set);

    /**
     * @brief 分割CSV行
     * @param line CSV行字符串
     * @return 分割后的字段向量
     */
    std::vector<std::string> splitCSVLine(const std::string& line) const;

    /**
     * @brief 更新静态参数到输出状态
     */
    void updateStaticParameters();

    /**
     * @brief 安全的状态设置（处理类型转换）
     * @param name 状态名称
     * @param value 状态值
     */
    void setStateValue(const std::string& name, const std::any& value);

    /**
     * @brief 处理基于飞行阶段的动态参数
     */
    void updatePhasedParameters();

    /**
     * @brief 处理基于高度的动态参数
     */
    void updateAltitudeBasedParameters();

private:
    // 当前激活的静态参数
    std::map<std::string, std::any> static_params_;
    
    // 多组参数集（从CSV加载）
    std::vector<std::map<std::string, std::any>> param_sets_;
    
    // 配置模式：single 或 csv
    std::string config_mode_;
    
    // 当前参数集索引
    size_t current_set_index_{0};
    
    // 动态参数的上次更新值（用于检测变化）
    std::map<std::string, std::any> last_dynamic_values_;
};

// 注册组件到工厂
static gnc::ComponentRegistrar<Disturbance> disturbance_registrar("Disturbance");
} // namespace gnc::components