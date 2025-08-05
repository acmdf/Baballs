#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <turbojpeg.h>

struct FrameData {
    unsigned char* pixels;
    int width;
    int height;
    int channels;
    size_t size;
    
    FrameData() : pixels(nullptr), width(0), height(0), channels(0), size(0) {}
    
    ~FrameData() {
        if (pixels) {
            tjFree(pixels);
            pixels = nullptr;
        }
    }
    
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;
    
    FrameData(FrameData&& other) noexcept
        : pixels(other.pixels), width(other.width), height(other.height), 
          channels(other.channels), size(other.size) {
        other.pixels = nullptr;
        other.width = 0;
        other.height = 0;
        other.channels = 0;
        other.size = 0;
    }
    
    FrameData& operator=(FrameData&& other) noexcept {
        if (this != &other) {
            if (pixels) {
                tjFree(pixels);
            }
            pixels = other.pixels;
            width = other.width;
            height = other.height;
            channels = other.channels;
            size = other.size;
            
            other.pixels = nullptr;
            other.width = 0;
            other.height = 0;
            other.channels = 0;
            other.size = 0;
        }
        return *this;
    }
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    
    bool LoadVideo(const std::string& filepath);
    void UnloadVideo();
    
    int GetVideoLength() const;
    std::unique_ptr<FrameData> GetNextFrame();
    void ResetPlayback();
    
    bool IsLoaded() const { return is_loaded_; }
    bool HasMoreFrames() const { return current_frame_ < frame_count_; }
    int GetCurrentFrameIndex() const { return current_frame_; }
    
    int GetVideoWidth() const { return video_width_; }
    int GetVideoHeight() const { return video_height_; }

private:
    struct FrameInfo {
        size_t offset;
        uint32_t size;
    };
    
    bool ParseFrameOffsets();
    std::unique_ptr<FrameData> DecompressFrame(const std::vector<unsigned char>& jpeg_data);
    
    tjhandle decompressor_;
    std::ifstream file_;
    std::vector<FrameInfo> frame_offsets_;
    
    bool is_loaded_;
    int frame_count_;
    int current_frame_;
    int video_width_;
    int video_height_;
    std::string filepath_;
};