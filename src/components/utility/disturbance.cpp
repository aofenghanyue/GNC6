/**
 * @file disturbance_component.cpp
 * @brief 拉偏参数管理组件实现
 */

#include "gnc/components/utility/disturbance.hpp"
#include "gnc/components/utility/config_manager.hpp"
#include "gnc/core/component_registrar.hpp"
#include "gnc/common/exceptions.hpp"
#include "math/math.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>

namespace gnc::components {

void Disturbance::declareCommonOutputs() {
    // IMU相关拉偏参数
    declareOutput<Vector3d>("imu_gyro_bias", Vector3d::Zero());
    declareOutput<Vector3d>("imu_accel_bias", Vector3d::Zero());
    declareOutput<double>("imu_gyro_noise_std", 0.001);
    declareOutput<double>("imu_accel_noise_std", 0.01);
    
    // GPS相关拉偏参数
    declareOutput<Vector3d>("gps_position_bias", Vector3d::Zero());
    declareOutput<Vector3d>("gps_velocity_bias", Vector3d::Zero());
    declareOutput<double>("gps_position_noise_std", 1.0);
    declareOutput<double>("gps_velocity_noise_std", 0.1);
    
    // 空气动力相关拉偏参数
    declareOutput<double>("drag_factor", 1.0);
    declareOutput<double>("lift_factor", 1.0);
    declareOutput<double>("side_force_factor", 1.0);
    
    // 控制相关拉偏参数
    declareOutput<double>("control_gain_factor", 1.0);
    declareOutput<Vector3d>("actuator_bias", Vector3d::Zero());
    
    // 环境相关拉偏参数
    declareOutput<double>("wind_factor", 1.0);
    declareOutput<double>("density_factor", 1.0);
    
    // 动力学相关拉偏参数
    declareOutput<double>("mass_factor", 1.0);
    declareOutput<Vector3d>("cg_offset", Vector3d::Zero());
    declareOutput<double>("thrust_factor", 1.0);
}

void Disturbance::initialize() {
    try {
        // 获取配置
        using namespace gnc::components::utility;
        auto& config_manager = ConfigManager::getInstance();
        auto utility_config = config_manager.getConfig(ConfigFileType::UTILITY);
        auto disturbance_config = utility_config["utility"]["disturbance"];
        
        // 确定配置模式
        config_mode_ = disturbance_config.value("mode", "single");
        
        if (config_mode_ == "single") {
            // 单组参数模式
            auto params = disturbance_config["parameters"];
            loadSingleParameters(params);
            
            std::cout << "[Disturbance] Loaded single parameter set with " 
                      << static_params_.size() << " parameters\n";
        }
        else if (config_mode_ == "csv") {
            // 多组参数模式
            std::string csv_file = disturbance_config.value("csv_file", "config/param_sets.csv");
            size_t active_set = disturbance_config.value("active_set", 0);
            
            loadCSVParameters(csv_file, active_set);
            
            std::cout << "[Disturbance] Loaded " << param_sets_.size() 
                      << " parameter sets from CSV, using set " << current_set_index_ << "\n";
        }
        else {
            throw std::runtime_error("[Disturbance] Unknown config mode: " + config_mode_);
        }
        
        // 将静态参数设置到输出状态
        updateStaticParameters();
        
    } catch (const std::exception& e) {
        std::cerr << "[Disturbance] Initialization failed: " << e.what() << std::endl;
        // 使用默认值继续运行
        std::cout << "[Disturbance] Using default parameter values\n";
    }
}

void Disturbance::updateImpl() {
    // 处理简单的动态参数逻辑
    
    // 1. 基于飞行阶段的参数调整
    updatePhasedParameters();
    
    // 2. 基于高度的参数调整
    updateAltitudeBasedParameters();
    
    // 可以在这里添加更多简单的动态逻辑
}

void Disturbance::loadSingleParameters(const nlohmann::json& params) {
    for (auto& [key, value] : params.items()) {
        try {
            if (value.is_number()) {
                static_params_[key] = value.get<double>();
            }
            else if (value.is_string()) {
                static_params_[key] = value.get<std::string>();
            }
            else if (value.is_array() && value.size() == 3) {
                // 处理三维向量
                Vector3d vec;
                vec.x() = value[0].get<double>();
                vec.y() = value[1].get<double>();
                vec.z() = value[2].get<double>();
                static_params_[key] = vec;
            }
            else {
                std::cerr << "[Disturbance] Unsupported parameter type for: " << key << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Disturbance] Error parsing parameter " << key 
                      << ": " << e.what() << std::endl;
        }
    }
}

void Disturbance::loadCSVParameters(const std::string& filename, size_t active_set) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("[Disturbance] Cannot open CSV file: " + filename);
    }
    
    std::string line;
    std::vector<std::string> headers;
    
    // 读取表头
    if (std::getline(file, line)) {
        headers = splitCSVLine(line);
        if (headers.empty()) {
            throw std::runtime_error("[Disturbance] Empty CSV header");
        }
    } else {
        throw std::runtime_error("[Disturbance] Cannot read CSV header");
    }
    
    // 读取数据行
    while (std::getline(file, line)) {
        auto values = splitCSVLine(line);
        if (values.size() != headers.size()) {
            std::cerr << "[Disturbance] CSV line length mismatch, skipping\n";
            continue;
        }
        
        std::map<std::string, std::any> param_set;
        
        // 跳过第一列（通常是SetID）
        for (size_t i = 1; i < headers.size(); ++i) {
            try {
                // 尝试解析为数字
                double num_val = std::stod(values[i]);
                param_set[headers[i]] = num_val;
            } catch (...) {
                // 如果不是数字，存储为字符串
                param_set[headers[i]] = values[i];
            }
        }
        
        param_sets_.push_back(param_set);
    }
    
    if (param_sets_.empty()) {
        throw std::runtime_error("[Disturbance] No parameter sets loaded from CSV");
    }
    
    // 设置当前激活的参数集
    selectParameterSet(active_set);
}

std::vector<std::string> Disturbance::splitCSVLine(const std::string& line) const {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // 去除前后空格
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        result.push_back(item);
    }
    
    return result;
}

void Disturbance::selectParameterSet(size_t set_index) {
    if (set_index >= param_sets_.size()) {
        std::cerr << "[Disturbance] Invalid parameter set index: " << set_index 
                  << " (max: " << param_sets_.size() - 1 << ")\n";
        return;
    }
    
    current_set_index_ = set_index;
    static_params_ = param_sets_[set_index];
    
    // 更新输出状态
    updateStaticParameters();
    
    std::cout << "[Disturbance] Switched to parameter set " << set_index 
              << " with " << static_params_.size() << " parameters\n";
}

void Disturbance::updateStaticParameters() {
    for (const auto& [name, value] : static_params_) {
        setStateValue(name, value);
    }
}

void Disturbance::setStateValue(const std::string& name, const std::any& value) {
    try {
        // 尝试不同的类型
        if (value.type() == typeid(double)) {
            setState(name, std::any_cast<double>(value));
        }
        else if (value.type() == typeid(Vector3d)) {
            setState(name, std::any_cast<Vector3d>(value));
        }
        else if (value.type() == typeid(std::string)) {
            // 字符串类型暂时跳过，或者可以添加字符串状态支持
            std::cout << "[Disturbance] Skipping string parameter: " << name << std::endl;
        }
        else {
            std::cerr << "[Disturbance] Unknown parameter type for: " << name << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Disturbance] Error setting parameter " << name 
                  << ": " << e.what() << std::endl;
    }
}

void Disturbance::updatePhasedParameters() {
    try {
        // 尝试获取当前飞行阶段
        auto phase = get<std::string>("FlowController.current_phase");
        
        // 基于阶段调整拉偏参数
        if (phase == "boost") {
            // 助推段：阻力增加20%
            double current_drag = last_dynamic_values_.count("drag_factor") ? 
                std::any_cast<double>(last_dynamic_values_["drag_factor"]) : 1.0;
            
            if (current_drag != 1.2) {
                setState("drag_factor", 1.2);
                last_dynamic_values_["drag_factor"] = 1.2;
            }
        }
        else if (phase == "coast" || phase == "terminal") {
            // 滑行段/末段：阻力减少20%
            double current_drag = last_dynamic_values_.count("drag_factor") ? 
                std::any_cast<double>(last_dynamic_values_["drag_factor"]) : 1.0;
            
            if (current_drag != 0.8) {
                setState("drag_factor", 0.8);
                last_dynamic_values_["drag_factor"] = 0.8;
            }
        }
        else {
            // 默认阶段：无拉偏
            double current_drag = last_dynamic_values_.count("drag_factor") ? 
                std::any_cast<double>(last_dynamic_values_["drag_factor"]) : 1.0;
            
            if (current_drag != 1.0) {
                setState("drag_factor", 1.0);
                last_dynamic_values_["drag_factor"] = 1.0;
            }
        }
        
    } catch (...) {
        // FlowController可能不存在或阶段状态不可用
        // 静默忽略，使用默认值
    }
}

void Disturbance::updateAltitudeBasedParameters() {
    try {
        // 尝试获取当前高度
        double altitude = get<double>("Dynamics.altitude");
        
        // 基于高度调整控制增益
        double gain_factor = 1.0;
        if (altitude > 50000.0) {
            // 高空：控制增益减少
            gain_factor = 0.8;
        } else if (altitude < 10000.0) {
            // 低空：控制增益增加
            gain_factor = 1.2;
        }
        
        double current_gain = last_dynamic_values_.count("control_gain_factor") ? 
            std::any_cast<double>(last_dynamic_values_["control_gain_factor"]) : 1.0;
        
        if (std::abs(current_gain - gain_factor) > 1e-6) {
            setState("control_gain_factor", gain_factor);
            last_dynamic_values_["control_gain_factor"] = gain_factor;
        }
        
    } catch (...) {
        // 高度状态不可用，使用默认值
    }
}

} // namespace gnc::components