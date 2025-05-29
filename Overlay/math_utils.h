#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <openvr/openvr.h>

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

#endif /* MATH_UTILS_H */