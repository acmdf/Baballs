#include "video_player.h"
#include <cstring>
#include <iostream>

VideoPlayer::VideoPlayer()
    : decompressor_(nullptr)
    , is_loaded_(false)
    , frame_count_(0)
    , current_frame_(0)
    , video_width_(0)
    , video_height_(0) {
    decompressor_ = tjInitDecompress();
    if (!decompressor_) {
        std::cerr << "Failed to initialize TurboJPEG decompressor: " << tjGetErrorStr() << std::endl;
    }
}

VideoPlayer::~VideoPlayer() {
    UnloadVideo();
    if (decompressor_) {
        tjDestroy(decompressor_);
    }
}

bool VideoPlayer::LoadVideo(const std::string& filepath) {
    UnloadVideo();

    if (!decompressor_) {
        std::cerr << "TurboJPEG decompressor not initialized" << std::endl;
        return false;
    }

    filepath_ = filepath;
    file_.open(filepath, std::ios::binary);

    if (!file_.is_open()) {
        std::cerr << "Failed to open video file: " << filepath << std::endl;
        return false;
    }

    if (!ParseFrameOffsets()) {
        std::cerr << "Failed to parse frame offsets" << std::endl;
        file_.close();
        return false;
    }

    is_loaded_ = true;
    current_frame_ = 0;

    if (frame_count_ > 0) {
        auto first_frame = GetNextFrame();
        if (first_frame) {
            video_width_ = first_frame->width;
            video_height_ = first_frame->height;
            ResetPlayback();
        }
    }

    std::cout << "Loaded video: " << frame_count_ << " frames, "
              << video_width_ << "x" << video_height_ << std::endl;

    return true;
}

void VideoPlayer::UnloadVideo() {
    if (file_.is_open()) {
        file_.close();
    }
    frame_offsets_.clear();
    is_loaded_ = false;
    frame_count_ = 0;
    current_frame_ = 0;
    video_width_ = 0;
    video_height_ = 0;
}

bool VideoPlayer::ParseFrameOffsets() {
    frame_offsets_.clear();
    file_.seekg(0, std::ios::beg);

    size_t current_offset = 0;

    while (file_.good()) {
        uint32_t frame_size;
        file_.read(reinterpret_cast<char*>(&frame_size), 4);

        if (file_.gcount() != 4) {
            break;
        }

        current_offset += 4;

        FrameInfo info;
        info.offset = current_offset;
        info.size = frame_size;
        frame_offsets_.push_back(info);

        file_.seekg(frame_size, std::ios::cur);
        current_offset += frame_size;
    }

    frame_count_ = static_cast<int>(frame_offsets_.size());
    file_.clear();

    return frame_count_ > 0;
}

std::unique_ptr<FrameData> VideoPlayer::DecompressFrame(const std::vector<unsigned char>& jpeg_data) {
    int width, height, subsamp, colorspace;

    if (tjDecompressHeader3(decompressor_, jpeg_data.data(), (unsigned long)jpeg_data.size(),
                            &width, &height, &subsamp, &colorspace)
        < 0) {
        std::cerr << "Failed to read JPEG header: " << tjGetErrorStr() << std::endl;
        return nullptr;
    }

    int pixel_format = TJPF_RGB;
    int channels = 3;

    auto frame = std::make_unique<FrameData>();
    frame->width = width;
    frame->height = height;
    frame->channels = channels;
    frame->size = width * height * channels;

    frame->pixels = tjAlloc((int)frame->size);
    if (!frame->pixels) {
        std::cerr << "Failed to allocate memory for frame" << std::endl;
        return nullptr;
    }

    if (tjDecompress2(decompressor_, jpeg_data.data(), (unsigned long)jpeg_data.size(),
                      frame->pixels, width, 0, height, pixel_format, 0)
        < 0) {
        std::cerr << "Failed to decompress JPEG: " << tjGetErrorStr() << std::endl;
        return nullptr;
    }

    return frame;
}

int VideoPlayer::GetVideoLength() const {
    return frame_count_;
}

std::unique_ptr<FrameData> VideoPlayer::GetNextFrame() {
    if (!is_loaded_ || current_frame_ >= frame_count_) {
        return nullptr;
    }

    const FrameInfo& info = frame_offsets_[current_frame_];

    file_.seekg(info.offset);
    if (file_.fail()) {
        std::cerr << "Failed to seek to frame " << current_frame_ << std::endl;
        return nullptr;
    }

    std::vector<unsigned char> jpeg_data(info.size);
    file_.read(reinterpret_cast<char*>(jpeg_data.data()), info.size);

    if (file_.gcount() != static_cast<std::streamsize>(info.size)) {
        std::cerr << "Failed to read frame data for frame " << current_frame_ << std::endl;
        return nullptr;
    }

    current_frame_++;

    return DecompressFrame(jpeg_data);
}

void VideoPlayer::ResetPlayback() {
    current_frame_ = 0;
}