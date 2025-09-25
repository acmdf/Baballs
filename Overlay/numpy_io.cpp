#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "numpy_io.h"

/**
 * Saves an array to a NumPy .npy file
 *
 * @param filename Path to save the .npy file
 * @param data Pointer to the data array
 * @param shape Vector containing dimensions (e.g., {1000,3} for 1000 3D points)
 * @param dataType Type of data being saved (FLOAT32, INT32, etc.)
 * @return true if successful, false otherwise
 */
bool NumPyIO::SaveArrayToNumpy(const std::string& filename, const void* data,
                               const std::vector<size_t>& shape, NumPyDataType dataType) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Get type information
    auto typeInfoIt = TYPE_INFO.find(dataType);
    if (typeInfoIt == TYPE_INFO.end()) {
        return false;
    }
    const TypeInfo& typeInfo = typeInfoIt->second;

    // Calculate total number of elements
    size_t totalElements = 1;
    for (size_t dim : shape) {
        totalElements *= dim;
    }

    // Build shape string for header
    std::string shapeStr = "(";
    for (size_t i = 0; i < shape.size(); ++i) {
        shapeStr += std::to_string(shape[i]);
        if (i < shape.size() - 1) {
            shapeStr += ", ";
        }
    }
    // For single-dimension arrays, add trailing comma to make it a tuple
    if (shape.size() == 1) {
        shapeStr += ",";
    }
    shapeStr += ")";

    // Check endianness
    uint16_t endianCheck = 1;
    char endianness = *reinterpret_cast<char*>(&endianCheck) ? '<' : '>';

    // Create header
    std::string header = "{'descr': '";
    header += endianness;
    header += typeInfo.numpyDescr;
    header += "', 'fortran_order': False, 'shape': ";
    header += shapeStr;
    header += "}";

    // Pad header to make total length divisible by 64 for alignment
    size_t headerLen = header.length() + 1;       // +1 for newline
    size_t padLen = 64 - ((headerLen + 10) % 64); // +10 for magic string, version, header length
    if (padLen < 64) {
        headerLen += padLen;
    }
    header.append(padLen, ' ');
    header += '\n';

    // Magic string for .npy format
    const char magic[] = "\x93NUMPY";
    file.write(magic, 6);

    // Version 1.0
    const uint8_t majorVersion = 1;
    const uint8_t minorVersion = 0;
    file.write(reinterpret_cast<const char*>(&majorVersion), 1);
    file.write(reinterpret_cast<const char*>(&minorVersion), 1);

    // Header length (little endian)
    uint16_t headerLenLE = static_cast<uint16_t>(header.length());
    file.write(reinterpret_cast<char*>(&headerLenLE), 2);

    // Header
    file.write(header.c_str(), header.length());

    // Data
    file.write(reinterpret_cast<const char*>(data), totalElements * typeInfo.size);

    return file.good();
}

/**
 * Reads a NumPy .npy file into an array
 *
 * @param filename Path to the .npy file to read
 * @param data Pointer to pre-allocated array to store the data (nullptr if you want the function to allocate)
 * @param shape Output vector that will contain the dimensions of the array
 * @param dataType Type of data to read (FLOAT32, INT32, etc.)
 * @return Pointer to the data (either the input pointer or newly allocated memory)
 * @throws std::runtime_error if file cannot be opened or has invalid format
 * @note If data is nullptr, memory is allocated and must be freed by the caller
 */
void* NumPyIO::ReadNumpyToArray(const std::string& filename, void* data,
                                std::vector<size_t>& shape, NumPyDataType dataType) {
    // Get type information
    auto typeInfoIt = TYPE_INFO.find(dataType);
    if (typeInfoIt == TYPE_INFO.end()) {
        throw std::runtime_error("Unsupported data type");
    }
    const TypeInfo& typeInfo = typeInfoIt->second;

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    // Check magic string
    char magic[6];
    file.read(magic, 6);
    if (memcmp(magic, "\x93NUMPY", 6) != 0) {
        throw std::runtime_error("Invalid NumPy file format (incorrect magic string)");
    }

    // Read version
    uint8_t majorVersion, minorVersion;
    file.read(reinterpret_cast<char*>(&majorVersion), 1);
    file.read(reinterpret_cast<char*>(&minorVersion), 1);

    // Read header length
    uint16_t headerLen;
    file.read(reinterpret_cast<char*>(&headerLen), 2);

    // Read header
    std::vector<char> headerBuf(headerLen + 1, 0); // +1 for null terminator
    file.read(headerBuf.data(), headerLen);
    std::string header(headerBuf.data());

    // Parse shape from header
    shape.clear();
    size_t shapeStart = header.find("'shape': (") + 10;
    size_t shapeEnd = header.find(")", shapeStart);
    std::string shapeStr = header.substr(shapeStart, shapeEnd - shapeStart);

    // Parse shape dimensions
    size_t pos = 0;
    while (pos < shapeStr.length()) {
        size_t commaPos = shapeStr.find(',', pos);
        if (commaPos == std::string::npos) {
            // Handle last number or single dimension
            std::string numStr = shapeStr.substr(pos);
            if (!numStr.empty() && numStr.find_first_not_of(" \t") != std::string::npos) {
                shape.push_back(std::stoull(numStr));
            }
            break;
        }

        std::string numStr = shapeStr.substr(pos, commaPos - pos);
        shape.push_back(std::stoull(numStr));
        pos = commaPos + 1;
    }

    // Verify we have a valid shape
    if (shape.empty()) {
        throw std::runtime_error("Failed to parse array shape from NumPy header");
    }

    // Calculate total elements
    size_t totalElements = 1;
    for (size_t dim : shape) {
        totalElements *= dim;
    }

    // Check data type from header
    std::string expectedType1 = "'descr': '<" + typeInfo.numpyDescr + "'";
    std::string expectedType2 = "'descr': '>" + typeInfo.numpyDescr + "'";

    if (header.find(expectedType1) == std::string::npos && header.find(expectedType2) == std::string::npos) {
        throw std::runtime_error("File contains incompatible data type for the requested read");
    }

    // Allocate memory if needed
    void* resultData = data;
    if (resultData == nullptr) {
        resultData = operator new(totalElements * typeInfo.size);
    }

    // Read the actual data
    file.read(reinterpret_cast<char*>(resultData), totalElements * typeInfo.size);

    // Check endianness
    bool fileIsLittleEndian = (header.find("'descr': '<") != std::string::npos);

    // Check if we need to swap endianness
    uint16_t endianCheck = 1;
    bool systemIsLittleEndian = (*reinterpret_cast<char*>(&endianCheck) == 1);

    // Swap endianness if needed
    if (fileIsLittleEndian != systemIsLittleEndian) {
        // For 4-byte types (float32, int32)
        if (typeInfo.size == 4) {
            uint32_t* values = reinterpret_cast<uint32_t*>(resultData);
            for (size_t i = 0; i < totalElements; i++) {
                values[i] = ((values[i] & 0xFF) << 24)
                          | ((values[i] & 0xFF00) << 8)
                          | ((values[i] & 0xFF0000) >> 8)
                          | ((values[i] & 0xFF000000) >> 24);
            }
        }
        // Add more cases for different sizes if needed
    }

    return resultData;
}

/**
 * Appends data to a NumPy .npy file or creates a new file if it doesn't exist
 *
 * @param filename Path to the .npy file
 * @param data Pointer to the data to append
 * @param elements Number of elements to append
 * @param dataType Type of data (FLOAT32, INT32, etc.)
 * @return true if successful, false otherwise
 */
bool NumPyIO::AppendToNumpyArray(const std::string& filename, const void* data,
                                 size_t elements, NumPyDataType dataType) {
    const size_t FIXED_HEADER_SIZE = 256; // Fixed size for header with padding

    // Get type information
    auto typeInfoIt = TYPE_INFO.find(dataType);
    if (typeInfoIt == TYPE_INFO.end()) {
        return false;
    }
    const TypeInfo& typeInfo = typeInfoIt->second;

    // Check if file exists
    size_t currentSize = 0;
    std::ifstream checkFile(filename, std::ios::binary);
    bool fileExists = checkFile.good();

    if (fileExists) {
        // Skip magic string, version, and header length (10 bytes)
        checkFile.seekg(10);

        // Read header length
        uint16_t headerLen;
        checkFile.read(reinterpret_cast<char*>(&headerLen), 2);

        // Skip the header
        checkFile.seekg(headerLen, std::ios::cur);

        // Calculate current size based on file size
        std::streampos dataStart = checkFile.tellg();
        checkFile.seekg(0, std::ios::end);
        std::streampos fileEnd = checkFile.tellg();

        currentSize = (fileEnd - dataStart) / typeInfo.size;
    }
    checkFile.close();

    if (!fileExists) {
        // Create new file
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Create new header
        writeHeader(file, elements, dataType, FIXED_HEADER_SIZE);

        // Write data
        file.write(reinterpret_cast<const char*>(data), elements * typeInfo.size);

        return file.good();
    } else {
        // Open for append
        std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) {
            return false;
        }

        // Calculate new size
        size_t newSize = currentSize + elements;

        // Rewrite the header with the new size
        file.seekp(0);
        writeHeader(file, newSize, dataType, FIXED_HEADER_SIZE);

        // Seek to end and append data
        file.seekp(0, std::ios::end);
        file.write(reinterpret_cast<const char*>(data), elements * typeInfo.size);

        return file.good();
    }
}

/**
 * Helper function to write a NumPy header with specified size
 */
void NumPyIO::writeHeader(std::ostream& file, size_t elements, NumPyDataType dataType, size_t fixedHeaderSize) {
    auto typeInfoIt = TYPE_INFO.find(dataType);
    const TypeInfo& typeInfo = typeInfoIt->second;

    // Check endianness
    uint16_t endianCheck = 1;
    char endianness = *reinterpret_cast<char*>(&endianCheck) ? '<' : '>';

    // Create header with 1D shape
    std::string header = "{'descr': '";
    header += endianness;
    header += typeInfo.numpyDescr;
    header += "', 'fortran_order': False, 'shape': (";
    header += std::to_string(elements);
    header += ",)}";

    // Pad header to fixed size (leave space for one newline)
    size_t paddingNeeded = fixedHeaderSize - 1 - header.length();
    header.append(paddingNeeded, ' ');
    header += '\n';

    // Magic string for .npy format
    const char magic[] = "\x93NUMPY";
    file.write(magic, 6);

    // Version 1.0
    const uint8_t majorVersion = 1;
    const uint8_t minorVersion = 0;
    file.write(reinterpret_cast<const char*>(&majorVersion), 1);
    file.write(reinterpret_cast<const char*>(&minorVersion), 1);

    // Header length (fixed)
    uint16_t headerLenLE = (uint16_t)fixedHeaderSize;
    file.write(reinterpret_cast<char*>(&headerLenLE), 2);

    // Header
    file.write(header.c_str(), header.length());
}
// For backward compatibility, provide the original function names
bool NumPyIO::SaveFloatArrayToNumpy(const std::string& filename, const float* data, const std::vector<size_t>& shape) {
    return SaveArrayToNumpy(filename, data, shape, NumPyDataType::FLOAT32);
}

float* NumPyIO::ReadNumpyToFloatArray(const std::string& filename, float* data, std::vector<size_t>& shape) {
    return static_cast<float*>(ReadNumpyToArray(filename, data, shape, NumPyDataType::FLOAT32));
}

// Add convenience functions for int32
bool NumPyIO::SaveInt32ArrayToNumpy(const std::string& filename, const int32_t* data, const std::vector<size_t>& shape) {
    return SaveArrayToNumpy(filename, data, shape, NumPyDataType::INT32);
}

int32_t* NumPyIO::ReadNumpyToInt32Array(const std::string& filename, int32_t* data, std::vector<size_t>& shape) {
    return static_cast<int32_t*>(ReadNumpyToArray(filename, data, shape, NumPyDataType::INT32));
}