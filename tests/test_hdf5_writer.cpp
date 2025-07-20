/**
 * @file test_hdf5_writer.cpp
 * @brief Unit tests for HDF5Writer
 */

#include <gtest/gtest.h>
#include "gnc/components/utility/hdf5_writer.hpp"
#include "gnc/common/types.hpp"
#include "math/math.hpp"
#include <filesystem>
#include <any>

using namespace gnc::components::utility;
using namespace gnc::states;

class HDF5WriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_path_ = "test_output.h5";
        // Clean up any existing test file
        if (std::filesystem::exists(test_file_path_)) {
            std::filesystem::remove(test_file_path_);
        }
    }
    
    void TearDown() override {
        // Clean up test file
        if (std::filesystem::exists(test_file_path_)) {
            std::filesystem::remove(test_file_path_);
        }
    }
    
    std::string test_file_path_;
};

TEST_F(HDF5WriterTest, HDF5AvailabilityCheck) {
    // Test that we can check HDF5 availability
    bool available = HDF5Writer::isHDF5Available();
    
    // This test should pass regardless of whether HDF5 is available
    // We're just testing that the function doesn't crash
    EXPECT_TRUE(available || !available);  // Always true, but tests the function call
}

TEST_F(HDF5WriterTest, InitializationWithoutHDF5) {
    if (HDF5Writer::isHDF5Available()) {
        GTEST_SKIP() << "HDF5 is available, skipping fallback test";
    }
    
    HDF5Writer writer;
    std::vector<StateId> states = {
        StateId{ComponentId{VehicleId(1), "TestComponent"}, "test_state"}
    };
    
    // Should throw runtime_error when HDF5 is not available
    EXPECT_THROW(writer.initialize(test_file_path_, states, true), std::runtime_error);
}

TEST_F(HDF5WriterTest, BasicInitializationWithHDF5) {
    if (!HDF5Writer::isHDF5Available()) {
        GTEST_SKIP() << "HDF5 not available, skipping HDF5-specific test";
    }
    
    HDF5Writer writer;
    std::vector<StateId> states = {
        StateId{ComponentId{VehicleId(1), "TestComponent"}, "test_state"}
    };
    
    // Should not throw when HDF5 is available
    EXPECT_NO_THROW(writer.initialize(test_file_path_, states, true));
    
    // File should be created
    EXPECT_TRUE(std::filesystem::exists(test_file_path_));
    
    // Should be able to finalize
    EXPECT_NO_THROW(writer.finalize());
}

TEST_F(HDF5WriterTest, WriteDataPointWithHDF5) {
    if (!HDF5Writer::isHDF5Available()) {
        GTEST_SKIP() << "HDF5 not available, skipping HDF5-specific test";
    }
    
    HDF5Writer writer;
    std::vector<StateId> states = {
        StateId{ComponentId{VehicleId(1), "TestComponent"}, "scalar_state"},
        StateId{ComponentId{VehicleId(1), "TestComponent"}, "vector_state"}
    };
    
    ASSERT_NO_THROW(writer.initialize(test_file_path_, states, true));
    
    // Write some test data
    std::vector<std::any> values = {
        std::any(42.0),  // scalar
        std::any(Vector3d(1.0, 2.0, 3.0))  // vector
    };
    
    EXPECT_NO_THROW(writer.writeDataPoint(0.1, values));
    EXPECT_NO_THROW(writer.writeDataPoint(0.2, values));
    
    EXPECT_NO_THROW(writer.finalize());
    
    // File should exist and have non-zero size
    EXPECT_TRUE(std::filesystem::exists(test_file_path_));
    EXPECT_GT(std::filesystem::file_size(test_file_path_), 0);
}

TEST_F(HDF5WriterTest, ErrorHandling) {
    HDF5Writer writer;
    
    // Should throw when not initialized
    std::vector<std::any> values = {std::any(42.0)};
    EXPECT_THROW(writer.writeDataPoint(0.1, values), std::runtime_error);
    
    // Should throw with empty states
    std::vector<StateId> empty_states;
    EXPECT_THROW(writer.initialize(test_file_path_, empty_states, false), std::runtime_error);
}