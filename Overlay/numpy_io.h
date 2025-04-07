#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Enum for supported NumPy data types
enum class NumPyDataType {
    FLOAT32,
    INT32,
    // Add more types as needed
};

// Structure to hold type information
struct TypeInfo {
    std::string numpyDescr;
    size_t size;
};

// Map to associate data types with their NumPy descriptor and size
static const std::unordered_map<NumPyDataType, TypeInfo> TYPE_INFO = {
    {NumPyDataType::FLOAT32, {"f4", sizeof(float)}},
    {NumPyDataType::INT32, {"i4", sizeof(int32_t)}},
    // Add more types as needed
};

class NumPyIO {
public:
    // Generic functions that support multiple data types
    static bool SaveArrayToNumpy(const std::string& filename, const void* data, 
                                const std::vector<size_t>& shape, NumPyDataType dataType);
    
    static void* ReadNumpyToArray(const std::string& filename, void* data, 
                                 std::vector<size_t>& shape, NumPyDataType dataType);
    
    // Original float-specific functions (for backward compatibility)
    static bool SaveFloatArrayToNumpy(const std::string& filename, const float* data, 
                                     const std::vector<size_t>& shape);
    
    static float* ReadNumpyToFloatArray(const std::string& filename, float* data, 
                                       std::vector<size_t>& shape);
    
    // New int32-specific convenience functions
    static bool SaveInt32ArrayToNumpy(const std::string& filename, const int32_t* data, 
                                     const std::vector<size_t>& shape);
    
    static int32_t* ReadNumpyToInt32Array(const std::string& filename, int32_t* data, 
                                         std::vector<size_t>& shape);

    static bool AppendToNumpyArray(const std::string& filename, const void* data, 
        size_t elements, NumPyDataType dataType);
    
private:
    static void writeHeader(std::ostream& file, size_t elements, NumPyDataType dataType, size_t fixedHeaderSize);
};