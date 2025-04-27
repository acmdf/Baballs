#include "frame_buffer.h"
#include "jpeg_stream.h"  // Include the correct header file
#include <cstring>        // for memcpy
#include <chrono>
#include <cstdlib>        // for malloc/free
#include <stdio.h>        // for printf

// Define STB_IMAGE_RESIZE_IMPLEMENTATION in exactly one source file
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"  // You'll need to download this header



#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
#endif
uint64_t current_time_ms_2(void) { // todo: utils class
    #ifdef _WIN32
        // Windows implementation
        LARGE_INTEGER frequency;
        LARGE_INTEGER count;
        
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&count);
        
        return (uint64_t)((count.QuadPart * 1000ULL) / frequency.QuadPart);
    #else
        // POSIX implementation
        struct timespec spec;
        
        if (clock_gettime(CLOCK_REALTIME, &spec) == 0) {
            return (uint64_t)(spec.tv_sec * 1000ULL + spec.tv_nsec / 1000000ULL);
        } else {
            // Fallback to gettimeofday if clock_gettime is not available
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (uint64_t)(tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL);
        }
    #endif
}

FrameBuffer::FrameBuffer(const char* url, int updateInterval)
    : streamUrl(url), 
      frontBuffer(0), 
      running(false),
      updateIntervalMs(updateInterval),
      frameInUse(false),
      targetWidth(0),
      targetHeight(0),
      resizeEnabled(false) {
    // Initialize empty buffers
    buffers[0].clear();
    buffers[1].clear();
}

FrameBuffer::FrameBuffer(const char* url, int targetWidth, int targetHeight, int updateInterval)
    : streamUrl(url), 
      frontBuffer(0), 
      running(false),
      updateIntervalMs(updateInterval),
      frameInUse(false),
      targetWidth(targetWidth),
      targetHeight(targetHeight),
      resizeEnabled(true) {
    // Initialize empty buffers
    buffers[0].clear();
    buffers[1].clear();
}

FrameBuffer::FrameBuffer(int targetWidth, int targetHeight, int updateInterval)
    : streamUrl(nullptr), 
      frontBuffer(0), 
      running(false),
      updateIntervalMs(updateInterval),
      frameInUse(false),
      targetWidth(targetWidth),
      targetHeight(targetHeight),
      resizeEnabled(true) {
    // Initialize empty buffers
    buffers[0].clear();
    buffers[1].clear();
}

FrameBuffer::~FrameBuffer() {
    stop();
    
    // Clean up buffers
    buffers[0].clear();
    buffers[1].clear();
}

void FrameBuffer::setURL(const char* url){
    this->streamUrl = url;
}

void FrameBuffer::setTargetResolution(int width, int height) {
    std::lock_guard<std::mutex> lock(frameMutex);
    targetWidth = width;
    targetHeight = height;
    resizeEnabled = (width > 0 && height > 0);
}

void FrameBuffer::start() {
    if (running.exchange(true)) {
        return; // Already running
    }
    
    // Connect to the stream - use the exact function from jpeg_stream.h
    printf("Getting JPEG stream handle to %s...", streamUrl);
    stream = GetStreamHandle(streamUrl);
    if (!stream) {
        running = false;
        printf("\nERROR: Can't get stream handle!\n");
        return;
    }
    printf(" [OK]\n");
    
    // Start update thread
    updateThread = std::thread(&FrameBuffer::updateLoop, this);
}

void FrameBuffer::stop() {
    if (!running.exchange(false)) {
        return; // Already stopped
    }
    
    // Wake up any waiting threads
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameCondition.notify_all();
    }
    
    // Wait for update thread to finish
    if (updateThread.joinable()) {
        updateThread.join();
    }
    
    // Close the stream
    if (stream) {
        CloseStream(stream);
        stream = nullptr;
    }
}

bool FrameBuffer::isRunning() const {
    return running;
}

int* FrameBuffer::resizeFrame(int* sourcePixels, int sourceWidth, int sourceHeight, 
                              int targetWidth, int targetHeight) {
    if (!sourcePixels || sourceWidth <= 0 || sourceHeight <= 0 || 
        targetWidth <= 0 || targetHeight <= 0) {
        return nullptr;
    }
    
    // Allocate target buffer
    int* targetPixels = static_cast<int*>(malloc(targetWidth * targetHeight * sizeof(int)));
    if (!targetPixels) {
        return nullptr;
    }
    
    // stb_image_resize expects RGBA data as unsigned char*
    // Since our pixels are stored as int (which are likely ARGB or RGBA format),
    // we can cast them to unsigned char* for the resize function
    
    int result = stbir_resize_uint8(
        reinterpret_cast<const unsigned char*>(sourcePixels), sourceWidth, sourceHeight, sourceWidth * sizeof(int),
        reinterpret_cast<unsigned char*>(targetPixels), targetWidth, targetHeight, targetWidth * sizeof(int),
        4  // Number of channels (RGBA)
    );
    
    if (result == 0) {
        // Resize failed
        free(targetPixels);
        return nullptr;
    }
    
    return targetPixels;
}

void FrameBuffer::updateLoop() {
    //printf("Update loop 1\n");
    while (running) {
        // Decode a new frame
        int width, height;
        //printf("Update loop 2\n");
        int* pixels = DecodeFrame(stream, &width, &height);
        //printf("Update loop 3\n");
        uint64_t time = current_time_ms_2();
        //printf("Update loop 4\n");
        if (pixels) {
            //printf("Update loop 5\n");
            // Get back buffer index
            int backBuffer = 1 - frontBuffer;
            
            // Clear old back buffer
            buffers[backBuffer].clear();
            
            // Check if we need to resize
            if (resizeEnabled && targetWidth > 0 && targetHeight > 0 && 
                (width != targetWidth || height != targetHeight)) {
                
                int* resizedPixels = resizeFrame(pixels, width, height, targetWidth, targetHeight);
                
                if (resizedPixels) {
                    // Free the original pixels since we made a copy
                    free(pixels);
                    
                    // Update back buffer with resized image
                    buffers[backBuffer].pixels = resizedPixels;
                    buffers[backBuffer].width = targetWidth;
                    buffers[backBuffer].height = targetHeight;
                } else {
                    // Resize failed, use original image
                    buffers[backBuffer].pixels = pixels;
                    buffers[backBuffer].width = width;
                    buffers[backBuffer].height = height;
                }
            } else {
                // No resize needed, update back buffer with original image
                buffers[backBuffer].pixels = pixels;
                buffers[backBuffer].width = width;
                buffers[backBuffer].height = height;
                
            }

            buffers[backBuffer].time = time;
            
            // Swap buffers (thread-safe operation)
            swapBuffers();
        }
        //printf("Update loop 6\n");
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(updateIntervalMs));
    }
}

void FrameBuffer::swapBuffers() {
    std::lock_guard<std::mutex> lock(frameMutex);
    
    // Only swap if the front buffer is not in use
    if (!frameInUse) {
        frontBuffer = 1 - frontBuffer;
    }
}

int* FrameBuffer::getFrameCopy(int* width, int* height, uint64_t* time) {
    std::lock_guard<std::mutex> lock(frameMutex);
    
    Frame& front = buffers[frontBuffer];
    
    if (!front.pixels) {
        *width = *height = 0;
        *time = 0;
        printf("Get Frame Copy returned nothing!\n");
        return nullptr;
    }
    
    // Allocate memory for the copy
    int pixelCount = front.width * front.height;
    int* copy = static_cast<int*>(malloc(pixelCount * sizeof(int)));
    
    if (copy) {
        // Copy the pixel data
        memcpy(copy, front.pixels, pixelCount * sizeof(int));
        *width = front.width;
        *height = front.height;
        *time = front.time;
    } else {
        *width = *height = 0;
        *time = 0;
    }

    
    
    return copy;
}

void FrameBuffer::lockFrame(int** pixels, int* width, int* height) {
    std::unique_lock<std::mutex> lock(frameMutex);
    
    // Wait until the frame is not in use
    while (frameInUse && running) {
        frameCondition.wait(lock);
    }
    
    // Mark frame as in use
    frameInUse = true;
    
    // Set output parameters
    Frame& front = buffers[frontBuffer];
    *pixels = front.pixels;
    *width = front.width;
    *height = front.height;
}

void FrameBuffer::unlockFrame() {
    std::lock_guard<std::mutex> lock(frameMutex);
    
    // Mark frame as no longer in use
    frameInUse = false;
    
    // Notify waiting threads
    frameCondition.notify_all();
}