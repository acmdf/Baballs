#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <openvr.h>

/* Vector structure */
struct MU_Vector3 {
    float x;
    float y;
    float z;
};

/* Matrix structure (4x4) */
struct MU_Matrix4 {
    float m[4][4];
};

/* Quaternion structure */
struct MU_Quaternion {
    float x;
    float y;
    float z;
    float w;
};

/* Angle conversion */
float MU_DegToRad(float degrees);
float MU_RadToDeg(float radians);

/* Vector operations */
struct MU_Vector3 MU_VectorAdd(struct MU_Vector3 a, struct MU_Vector3 b);
struct MU_Vector3 MU_VectorSubtract(struct MU_Vector3 a, struct MU_Vector3 b);
struct MU_Vector3 MU_VectorMultiply(struct MU_Vector3 v, float scalar);
float MU_VectorDot(struct MU_Vector3 a, struct MU_Vector3 b);
struct MU_Vector3 MU_VectorCross(struct MU_Vector3 a, struct MU_Vector3 b);
float MU_VectorLength(struct MU_Vector3 v);
struct MU_Vector3 MU_VectorNormalize(struct MU_Vector3 v);

/* Matrix operations */
struct MU_Matrix4 MU_MatrixIdentity(void);
struct MU_Matrix4 MU_MatrixMultiply(struct MU_Matrix4 a, struct MU_Matrix4 b);
struct MU_Vector3 MU_MatrixGetPosition(struct MU_Matrix4 m);

/* SteamVR matrix conversion */
struct MU_Matrix4 MU_ConvertSteamVRMatrixToMatrix4(vr::HmdMatrix34_t matPose);
vr::HmdMatrix34_t MU_ConvertMatrix4ToSteamVRMatrix(struct MU_Matrix4 m);

/* Matrix creation functions */
struct MU_Matrix4 MU_CreateRotationX(float angleRad);
struct MU_Matrix4 MU_CreateRotationY(float angleRad);
struct MU_Matrix4 MU_CreateRotationZ(float angleRad);
struct MU_Matrix4 MU_CreateTranslation(struct MU_Vector3 v);
struct MU_Matrix4 MU_CreateLookAtMatrix(struct MU_Vector3 eye, struct MU_Vector3 target, struct MU_Vector3 up);
struct MU_Matrix4 MU_CreateTransformMatrix(struct MU_Vector3 position, struct MU_Matrix4 rotation);

/* Quaternion operations */
struct MU_Quaternion MU_CreateQuaternionFromEuler(float pitch, float yaw, float roll);
struct MU_Matrix4 MU_CreateMatrixFromQuaternion(struct MU_Quaternion q);

/* Angle calculations */
float MU_CalculateYawAngle(struct MU_Vector3 v1, struct MU_Vector3 v2);
float MU_CalculatePitchAngle(struct MU_Vector3 v1, struct MU_Vector3 v2);

/* Eye gaze calculation structure */
struct MU_EyeGaze {
    float leftEyePitch;   // Left eye pitch angle in degrees
    float leftEyeYaw;     // Left eye yaw angle in degrees
    float rightEyePitch;  // Right eye pitch angle in degrees
    float rightEyeYaw;    // Right eye yaw angle in degrees
};

/* Unified gaze representation structure */
struct MU_UnifiedGaze {
    float pitch;          // Average pitch angle in degrees
    float yaw;            // Average yaw angle in degrees
    float convergence;    // Convergence value from 0.0 (parallel/divergent) to 1.0 (maximum convergence)
};

/* Convergence calculation parameters - adjustable for tuning */
struct MU_ConvergenceParams {
    float maxConvergenceAngle;    // Maximum expected convergence angle in degrees (default: ~6°)
    float minConvergenceDistance; // Minimum focus distance in meters (default: 0.1m)
    float maxConvergenceDistance; // Maximum focus distance in meters (default: 10.0m)
    float ipdMeters;             // Interpupillary distance in meters (default: 0.064m)
};

/* Eye gaze calculation function */
struct MU_EyeGaze MU_CalculateEyeGaze(
    struct MU_Vector3 hmdPosition,
    struct MU_Matrix4 hmdRotation,
    struct MU_Vector3 leftEyeOffset,
    struct MU_Vector3 rightEyeOffset,
    struct MU_Vector3 targetPosition
);

/* Convert individual eye gaze to unified pitch/yaw/convergence representation */
struct MU_UnifiedGaze MU_ConvertToUnifiedGaze(
    struct MU_EyeGaze eyeGaze,
    struct MU_ConvergenceParams params
);

/* Convert unified gaze back to individual eye gaze (inverse function) */
struct MU_EyeGaze MU_ConvertFromUnifiedGaze(
    struct MU_UnifiedGaze unifiedGaze,
    struct MU_ConvergenceParams params
);

/* Create default convergence parameters */
struct MU_ConvergenceParams MU_CreateDefaultConvergenceParams(void);

#endif /* MATH_UTILS_H */