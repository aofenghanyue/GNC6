// main.cpp
#include "gnc/core/simulator.hpp"
#include "gnc/components/utility/simple_logger.hpp"
#include <iostream>

int main() {
    try {
        // 1. 创建仿真器实例
        gnc::core::Simulator simulator;
        // 2. 初始化仿真环境（加载配置、创建组件等）
        simulator.initialize();

        // 3. 运行仿真
        simulator.run();

    } catch (const std::exception& e) {
        LOG_CRITICAL("An unhandled exception occurred in main: {}", e.what());
        // 确保即使在异常情况下也能尝试关闭日志
        gnc::components::utility::SimpleLogger::getInstance().shutdown();
        return 1;
    }

    // 4. 正常结束，关闭日志系统
    LOG_INFO("Program terminating normally.");
    gnc::components::utility::SimpleLogger::getInstance().shutdown();
    return 0;
}
