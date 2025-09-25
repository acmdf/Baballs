#pragma once

typedef struct CaptureFrame {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

    // Eye tracking parameters - Unified representation (average gaze)
    float routinePitch;       // Average pitch angle in degrees
    float routineYaw;         // Average yaw angle in degrees
    float routineDistance;    // Physical distance to target in meters
    float routineConvergence; // Convergence value from 0.0 to 1.0
    float fovAdjustDistance;  // FOV adjustment distance

    // Individual eye gaze parameters
    float leftEyePitch;  // Left eye pitch angle in degrees
    float leftEyeYaw;    // Left eye yaw angle in degrees
    float rightEyePitch; // Right eye pitch angle in degrees
    float rightEyeYaw;   // Right eye yaw angle in degrees

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
    uint32_t routineState; // Flags (see flags.h)
    uint32_t jpeg_data_left_length;
    uint32_t jpeg_data_right_length;

#ifdef _MSC_VER
    #pragma pack(pop)
#else
} __attribute__((packed)) CaptureFrame;
#endif

#ifdef _MSC_VER
} CaptureFrame;
#endif
