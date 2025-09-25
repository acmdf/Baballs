// capture_reader.h
#pragma once

#include "capture_data.h"
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

// Define the structure for our aligned frames
struct AlignedFrame {
    std::tuple<float, float, float, float, float, float, float, float, float, float, float, uint32_t> label_data; // (pitch, yaw, distance, fovAdjust, leftLid, rightLid, browRaise, browAngry, widen, squint, dilate, state)

    std::vector<uint8_t> left_image;  // JPEG data
    std::vector<uint8_t> right_image; // JPEG data
    uint64_t label_timestamp;

    // Decode the left eye image to RGB pixels
    bool DecodeImageLeft(std::vector<uint32_t>& rgb_buffer, int& width, int& height) const;

    // Decode the right eye image to RGB pixels
    bool DecodeImageRight(std::vector<uint32_t>& rgb_buffer, int& width, int& height) const;

private:
    // Helper method for JPEG decoding to avoid code duplication
    bool DecodeJpegData(const std::vector<uint8_t>& jpeg_data,
                        std::vector<uint32_t>& rgb_buffer,
                        int& width,
                        int& height) const;
    void FillJpegData(const std::vector<uint8_t>& jpeg_data,
                      std::vector<uint32_t>& rgb_buffer,
                      int& width, int& height,
                      int cached_width, int cached_height,
                      const std::vector<uint32_t>& cached_buffer) const;

    mutable bool has_decoded_left = false;
    mutable bool has_decoded_right = false;
    mutable int left_width = 0;
    mutable int left_height = 0;
    mutable int right_width = 0;
    mutable int right_height = 0;
    mutable std::vector<uint32_t> rgb_buffer_left;
    mutable std::vector<uint32_t> rgb_buffer_right;
};

// Main function to read and process a capture file
std::vector<AlignedFrame> read_capture_file(const std::string& filename);

// Helper function to extract label components
inline void extract_label_data(const AlignedFrame& frame,
                               float& pitch, float& yaw, float& distance,
                               float& fov_adjust, float& leftLid, float& rightLid,
                               float& browRaise, float& browAngry, float& widen,
                               float& squint, float& dilate, uint32_t& state) {
    std::tie(pitch, yaw, distance, fov_adjust, leftLid, rightLid, browRaise, browAngry, widen, squint, dilate, state) = frame.label_data;
}