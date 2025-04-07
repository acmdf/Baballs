#pragma once

typedef struct CaptureFrame {
    uint32_t image_data_left[128*128];
    uint32_t image_data_right[128*128];
    float routinePitch;    // Scaled by FLOAT_TO_INT_CONSTANT
    float routineYaw;      // Scaled by FLOAT_TO_INT_CONSTANT
    float routineDistance; // Target distance for convergence, scaled by FLOAT_TO_INT_CONSTANT
    float fovAdjustDistance; // FOV adjustment distance
    uint32_t timestampLow;    // Low 32 bits of timestamp
    uint32_t timestampHigh;   // High 32 bits of timestamp
    uint32_t routineState;    // Flags (see flags.h)
} CaptureFrame;

