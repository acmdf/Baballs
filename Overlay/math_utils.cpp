#include "math_utils.h"
#include <cmath>

/* Angle conversion */
float MU_DegToRad(float degrees)
{
    return degrees * 0.01745329251994329576923690768489f; /* PI / 180 */
}

float MU_RadToDeg(float radians)
{
    return radians * 57.295779513082320876798154814105f; /* 180 / PI */
}

/* Vector operations */
MU_Vector3 MU_VectorAdd(MU_Vector3 a, MU_Vector3 b)
{
    MU_Vector3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

MU_Vector3 MU_VectorSubtract(MU_Vector3 a, MU_Vector3 b)
{
    MU_Vector3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

MU_Vector3 MU_VectorMultiply(MU_Vector3 v, float scalar)
{
    MU_Vector3 result;
    result.x = v.x * scalar;
    result.y = v.y * scalar;
    result.z = v.z * scalar;
    return result;
}

float MU_VectorDot(MU_Vector3 a, MU_Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

MU_Vector3 MU_VectorCross(MU_Vector3 a, MU_Vector3 b)
{
    MU_Vector3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

float MU_VectorLength(MU_Vector3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

MU_Vector3 MU_VectorNormalize(MU_Vector3 v)
{
    float length = MU_VectorLength(v);
    MU_Vector3 result = {0.0f, 0.0f, 0.0f};
    
    if (length > 0.000001f) {
        float invLength = 1.0f / length;
        result.x = v.x * invLength;
        result.y = v.y * invLength;
        result.z = v.z * invLength;
    }
    
    return result;
}

/* Matrix operations */
MU_Matrix4 MU_MatrixIdentity(void)
{
    MU_Matrix4 result = {0};
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    return result;
}

MU_Matrix4 MU_MatrixMultiply(MU_Matrix4 a, MU_Matrix4 b)
{
    MU_Matrix4 result = {0};
    int i, j, k;
    
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    
    return result;
}

MU_Vector3 MU_MatrixGetPosition(MU_Matrix4 m)
{
    MU_Vector3 result = {m.m[0][3], m.m[1][3], m.m[2][3]};
    return result;
}

/* SteamVR matrix conversion */
MU_Matrix4 MU_ConvertSteamVRMatrixToMatrix4(vr::HmdMatrix34_t matPose)
{
    MU_Matrix4 result = {0};
    
    result.m[0][0] = matPose.m[0][0];
    result.m[0][1] = matPose.m[0][1];
    result.m[0][2] = matPose.m[0][2];
    result.m[0][3] = matPose.m[0][3];
    
    result.m[1][0] = matPose.m[1][0];
    result.m[1][1] = matPose.m[1][1];
    result.m[1][2] = matPose.m[1][2];
    result.m[1][3] = matPose.m[1][3];
    
    result.m[2][0] = matPose.m[2][0];
    result.m[2][1] = matPose.m[2][1];
    result.m[2][2] = matPose.m[2][2];
    result.m[2][3] = matPose.m[2][3];
    
    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = 0.0f;
    result.m[3][3] = 1.0f;
    
    return result;
}

vr::HmdMatrix34_t MU_ConvertMatrix4ToSteamVRMatrix(MU_Matrix4 m)
{
    vr::HmdMatrix34_t result;
    
    result.m[0][0] = m.m[0][0];
    result.m[0][1] = m.m[0][1];
    result.m[0][2] = m.m[0][2];
    result.m[0][3] = m.m[0][3];
    
    result.m[1][0] = m.m[1][0];
    result.m[1][1] = m.m[1][1];
    result.m[1][2] = m.m[1][2];
    result.m[1][3] = m.m[1][3];
    
    result.m[2][0] = m.m[2][0];
    result.m[2][1] = m.m[2][1];
    result.m[2][2] = m.m[2][2];
    result.m[2][3] = m.m[2][3];
    
    return result;
}

/* Matrix creation functions */
MU_Matrix4 MU_CreateRotationX(float angleRad)
{
    MU_Matrix4 result = MU_MatrixIdentity();
    
    float s = std::sin(angleRad);
    float c = std::cos(angleRad);
    
    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;
    
    return result;
}

MU_Matrix4 MU_CreateRotationY(float angleRad)
{
    MU_Matrix4 result = MU_MatrixIdentity();
    
    float s = std::sin(angleRad);
    float c = std::cos(angleRad);
    
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;
    
    return result;
}

MU_Matrix4 MU_CreateRotationZ(float angleRad)
{
    MU_Matrix4 result = MU_MatrixIdentity();
    
    float s = std::sin(angleRad);
    float c = std::cos(angleRad);
    
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    
    return result;
}

MU_Matrix4 MU_CreateTranslation(MU_Vector3 v)
{
    MU_Matrix4 result = MU_MatrixIdentity();
    
    result.m[0][3] = v.x;
    result.m[1][3] = v.y;
    result.m[2][3] = v.z;
    
    return result;
}

MU_Matrix4 MU_CreateLookAtMatrix(MU_Vector3 eye, MU_Vector3 target, MU_Vector3 up)
{
    MU_Vector3 zAxis = MU_VectorNormalize(MU_VectorSubtract(eye, target));  /* Forward */
    MU_Vector3 xAxis = MU_VectorNormalize(MU_VectorCross(up, zAxis));       /* Right */
    MU_Vector3 yAxis = MU_VectorCross(zAxis, xAxis);                        /* Up */
    
    MU_Matrix4 result = {0};
    
    result.m[0][0] = xAxis.x;
    result.m[0][1] = xAxis.y;
    result.m[0][2] = xAxis.z;
    result.m[0][3] = -MU_VectorDot(xAxis, eye);
    
    result.m[1][0] = yAxis.x;
    result.m[1][1] = yAxis.y;
    result.m[1][2] = yAxis.z;
    result.m[1][3] = -MU_VectorDot(yAxis, eye);
    
    result.m[2][0] = zAxis.x;
    result.m[2][1] = zAxis.y;
    result.m[2][2] = zAxis.z;
    result.m[2][3] = -MU_VectorDot(zAxis, eye);
    
    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = 0.0f;
    result.m[3][3] = 1.0f;
    
    return result;
}

MU_Matrix4 MU_CreateTransformMatrix(MU_Vector3 position, MU_Matrix4 rotation)
{
    MU_Matrix4 result = rotation;
    
    result.m[0][3] = position.x;
    result.m[1][3] = position.y;
    result.m[2][3] = position.z;
    
    return result;
}

/* Quaternion operations */
MU_Quaternion MU_CreateQuaternionFromEuler(float pitch, float yaw, float roll)
{
    /* Convert to radians */
    float p = pitch * 0.5f;
    float y = yaw * 0.5f;
    float r = roll * 0.5f;
    
    float sinp = std::sin(p);
    float siny = std::sin(y);
    float sinr = std::sin(r);
    float cosp = std::cos(p);
    float cosy = std::cos(y);
    float cosr = std::cos(r);
    
    MU_Quaternion q;
    q.x = sinr * cosp * cosy - cosr * sinp * siny;
    q.y = cosr * sinp * cosy + sinr * cosp * siny;
    q.z = cosr * cosp * siny - sinr * sinp * cosy;
    q.w = cosr * cosp * cosy + sinr * sinp * siny;
    
    return q;
}

MU_Matrix4 MU_CreateMatrixFromQuaternion(MU_Quaternion q)
{
    MU_Matrix4 result = MU_MatrixIdentity();
    
    float xx = q.x * q.x;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float xw = q.x * q.w;
    
    float yy = q.y * q.y;
    float yz = q.y * q.z;
    float yw = q.y * q.w;
    
    float zz = q.z * q.z;
    float zw = q.z * q.w;
    
    result.m[0][0] = 1.0f - 2.0f * (yy + zz);
    result.m[0][1] = 2.0f * (xy - zw);
    result.m[0][2] = 2.0f * (xz + yw);
    
    result.m[1][0] = 2.0f * (xy + zw);
    result.m[1][1] = 1.0f - 2.0f * (xx + zz);
    result.m[1][2] = 2.0f * (yz - xw);
    
    result.m[2][0] = 2.0f * (xz - yw);
    result.m[2][1] = 2.0f * (yz + xw);
    result.m[2][2] = 1.0f - 2.0f * (xx + yy);
    
    return result;
}

/* Angle calculations */
float MU_CalculateYawAngle(MU_Vector3 v1, MU_Vector3 v2)
{
    /* Project vectors onto XZ plane */
    MU_Vector3 v1xz = {v1.x, 0.0f, v1.z};
    MU_Vector3 v2xz = {v2.x, 0.0f, v2.z};
    
    v1xz = MU_VectorNormalize(v1xz);
    v2xz = MU_VectorNormalize(v2xz);
    
    /* Calculate angle using dot product */
    float dot = MU_VectorDot(v1xz, v2xz);
    dot = (dot < -1.0f) ? -1.0f : (dot > 1.0f) ? 1.0f : dot;
    
    float angle = MU_RadToDeg(std::acos(dot));
    
    /* Determine direction (left/right) */
    MU_Vector3 cross = MU_VectorCross(v1xz, v2xz);
    if (cross.y < 0.0f) {
        angle = -angle;
    }
    
    return angle;
}

float MU_CalculatePitchAngle(MU_Vector3 v1, MU_Vector3 v2)
{
    /* Calculate the pitch angle (up/down) */
    MU_Vector3 v1n = MU_VectorNormalize(v1);
    MU_Vector3 v2n = MU_VectorNormalize(v2);
    
    /* Calculate angle between vector and its projection on XZ plane */
    float v1pitch = MU_RadToDeg(std::asin(v1n.y));
    float v2pitch = MU_RadToDeg(std::asin(v2n.y));
    
    return v2pitch - v1pitch;
}