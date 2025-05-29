#pragma once
#include <cstdint>

typedef struct CaptureFrame {
#ifdef _MSC_VER
    #pragma pack(push, 1)
#endif

    // Eye tracking parameters
    float routinePitch;       // Scaled by FLOAT_TO_INT_CONSTANT
    float routineYaw;         // Scaled by FLOAT_TO_INT_CONSTANT
    float routineDistance;    // Target distance for convergence, scaled by FLOAT_TO_INT_CONSTANT
    float fovAdjustDistance;  // FOV adjustment distance

    // lid/brow
    float routineLeftLid;
    float routineRightLid;
    float routineBrowRaise; // surprise
    float routineBrowAngry; // lower
    float routineWiden;
    float routineSquint;
    float routineDilate; // display fully black screen, fully white screen. This should trigger dilation
    
    // Timestamp information
    uint64_t timestamp;
    uint64_t timestamp_left;
    uint64_t timestamp_right;
    
    // State and data size information
    uint32_t routineState;    // Flags (see flags.h)
    uint32_t jpeg_data_left_length;
    uint32_t jpeg_data_right_length;

#ifdef _MSC_VER
    #pragma pack(pop)
#endif
} CaptureFrame;
