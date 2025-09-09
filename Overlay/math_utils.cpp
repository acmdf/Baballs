#include "math_utils.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

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
    MU_Vector3 result = { 0.0f, 0.0f, 0.0f };

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
    MU_Matrix4 result = { 0 };
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    return result;
}

MU_Matrix4 MU_MatrixMultiply(MU_Matrix4 a, MU_Matrix4 b)
{
    MU_Matrix4 result = { 0 };
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
    MU_Vector3 result = { m.m[0][3], m.m[1][3], m.m[2][3] };
    return result;
}

/* SteamVR matrix conversion */
MU_Matrix4 MU_ConvertSteamVRMatrixToMatrix4(vr::HmdMatrix34_t matPose)
{
    MU_Matrix4 result = { 0 };

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

    MU_Matrix4 result = { 0 };

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
float MU_CalculateYawAngle(MU_Vector3 forward, MU_Vector3 target)
{
    /* Project both vectors onto XZ plane (horizontal plane) */
    MU_Vector3 forwardXZ = { forward.x, 0.0f, forward.z };
    MU_Vector3 targetXZ = { target.x, 0.0f, target.z };

    forwardXZ = MU_VectorNormalize(forwardXZ);
    targetXZ = MU_VectorNormalize(targetXZ);

    /* Calculate signed angle using atan2 for proper quadrant handling */
    float forwardAngle = atan2(forwardXZ.x, -forwardXZ.z);
    float targetAngle = atan2(targetXZ.x, -targetXZ.z);

    float yawAngle = targetAngle - forwardAngle;

    /* Normalize to [-π, π] range */
    while (yawAngle > M_PI) yawAngle -= 2.0f * M_PI;
    while (yawAngle < -M_PI) yawAngle += 2.0f * M_PI;

    return MU_RadToDeg(yawAngle);
}

float MU_CalculatePitchAngle(MU_Vector3 forward, MU_Vector3 target)
{
    /* Calculate the pitch angle (up/down) */
    MU_Vector3 forwardN = MU_VectorNormalize(forward);
    MU_Vector3 targetN = MU_VectorNormalize(target);

    /* Calculate the elevation angle of each vector */
    float forwardPitch = asin(forwardN.y);
    float targetPitch = asin(targetN.y);

    /* Return the difference in radians converted to degrees */
    return MU_RadToDeg(targetPitch - forwardPitch);
}

/* Eye gaze calculation function */
MU_EyeGaze MU_CalculateEyeGaze(
    MU_Vector3 hmdPosition,
    MU_Matrix4 hmdRotation,
    MU_Vector3 leftEyeOffset,
    MU_Vector3 rightEyeOffset,
    MU_Vector3 targetPosition)
{
    MU_EyeGaze result = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Transform eye offsets from HMD local space to world space
    // Left eye position in world space
    MU_Vector3 leftEyeWorld = {
        hmdPosition.x + (hmdRotation.m[0][0] * leftEyeOffset.x + hmdRotation.m[0][1] * leftEyeOffset.y + hmdRotation.m[0][2] * leftEyeOffset.z),
        hmdPosition.y + (hmdRotation.m[1][0] * leftEyeOffset.x + hmdRotation.m[1][1] * leftEyeOffset.y + hmdRotation.m[1][2] * leftEyeOffset.z),
        hmdPosition.z + (hmdRotation.m[2][0] * leftEyeOffset.x + hmdRotation.m[2][1] * leftEyeOffset.y + hmdRotation.m[2][2] * leftEyeOffset.z)
    };

    // Right eye position in world space
    MU_Vector3 rightEyeWorld = {
        hmdPosition.x + (hmdRotation.m[0][0] * rightEyeOffset.x + hmdRotation.m[0][1] * rightEyeOffset.y + hmdRotation.m[0][2] * rightEyeOffset.z),
        hmdPosition.y + (hmdRotation.m[1][0] * rightEyeOffset.x + hmdRotation.m[1][1] * rightEyeOffset.y + hmdRotation.m[1][2] * rightEyeOffset.z),
        hmdPosition.z + (hmdRotation.m[2][0] * rightEyeOffset.x + hmdRotation.m[2][1] * rightEyeOffset.y + hmdRotation.m[2][2] * rightEyeOffset.z)
    };

    // Calculate direction vectors from each eye to the target
    MU_Vector3 leftEyeToTarget = MU_VectorSubtract(targetPosition, leftEyeWorld);
    MU_Vector3 rightEyeToTarget = MU_VectorSubtract(targetPosition, rightEyeWorld);

    // DEBUG: Print raw values
    printf("DEBUG: HMD pos (%.3f, %.3f, %.3f)\n", hmdPosition.x, hmdPosition.y, hmdPosition.z);
    printf("DEBUG: Target pos (%.3f, %.3f, %.3f)\n", targetPosition.x, targetPosition.y, targetPosition.z);
    printf("DEBUG: LeftEye world (%.3f, %.3f, %.3f)\n", leftEyeWorld.x, leftEyeWorld.y, leftEyeWorld.z);
    printf("DEBUG: LeftEye->Target raw (%.3f, %.3f, %.3f)\n", leftEyeToTarget.x, leftEyeToTarget.y, leftEyeToTarget.z);

    // Normalize the direction vectors
    leftEyeToTarget = MU_VectorNormalize(leftEyeToTarget);
    rightEyeToTarget = MU_VectorNormalize(rightEyeToTarget);

    printf("DEBUG: LeftEye->Target norm (%.3f, %.3f, %.3f)\n", leftEyeToTarget.x, leftEyeToTarget.y, leftEyeToTarget.z);

    // Calculate HMD forward direction (negative Z in HMD local space)
    MU_Vector3 hmdForward = { -hmdRotation.m[0][2], -hmdRotation.m[1][2], -hmdRotation.m[2][2] };
    hmdForward = MU_VectorNormalize(hmdForward);

    printf("DEBUG: HMD forward (%.3f, %.3f, %.3f)\n", hmdForward.x, hmdForward.y, hmdForward.z);

    // Calculate gaze angles for left eye
    // Yaw: horizontal angle between HMD forward and eye-to-target direction
    result.leftEyeYaw = MU_CalculateYawAngle(hmdForward, leftEyeToTarget);

    // Pitch: vertical angle between HMD forward and eye-to-target direction  
    result.leftEyePitch = MU_CalculatePitchAngle(hmdForward, leftEyeToTarget);

    // Calculate gaze angles for right eye
    // Yaw: horizontal angle between HMD forward and eye-to-target direction
    result.rightEyeYaw = MU_CalculateYawAngle(hmdForward, rightEyeToTarget);

    // Pitch: vertical angle between HMD forward and eye-to-target direction
    result.rightEyePitch = MU_CalculatePitchAngle(hmdForward, rightEyeToTarget);

    printf("DEBUG: Final angles - Left(%.1f°, %.1f°) Right(%.1f°, %.1f°)\n",
        result.leftEyeYaw, result.leftEyePitch, result.rightEyeYaw, result.rightEyePitch);

    return result;
}

/* Helper function to convert pitch/yaw angles to 3D direction vector */
static MU_Vector3 MU_AnglesToDirection(float pitch, float yaw)
{
    float pitchRad = MU_DegToRad(pitch);
    float yawRad = MU_DegToRad(yaw);

    MU_Vector3 direction;
    direction.x = sin(yawRad) * cos(pitchRad);
    direction.y = sin(pitchRad);
    direction.z = -cos(yawRad) * cos(pitchRad);  // Negative Z is forward

    return MU_VectorNormalize(direction);
}

/* Helper function to convert 3D direction vector to pitch/yaw angles */
static void MU_DirectionToAngles(MU_Vector3 direction, float* pitch, float* yaw)
{
    MU_Vector3 dir = MU_VectorNormalize(direction);

    *pitch = MU_RadToDeg(asin(dir.y));
    *yaw = MU_RadToDeg(atan2(dir.x, -dir.z));
}

/* Create default convergence parameters */
MU_ConvergenceParams MU_CreateDefaultConvergenceParams(void)
{
    MU_ConvergenceParams params;
    params.maxConvergenceAngle = 45.0f;     // Maximum convergence angle in degrees (fully cross-eyed)
    params.minConvergenceDistance = 0.02f;  // 2cm minimum focus distance (nose touching)
    params.maxConvergenceDistance = 10.0f;  // 10m maximum focus distance (beyond this is essentially parallel)
    params.ipdMeters = 0.064f;              // Average IPD of 64mm
    return params;
}

/* Convert individual eye gaze to unified pitch/yaw/convergence representation */
MU_UnifiedGaze MU_ConvertToUnifiedGaze(MU_EyeGaze eyeGaze, MU_ConvergenceParams params)
{
    MU_UnifiedGaze result;

    // Convert pitch/yaw angles to 3D direction vectors for each eye
    MU_Vector3 leftGazeDir = MU_AnglesToDirection(eyeGaze.leftEyePitch, eyeGaze.leftEyeYaw);
    MU_Vector3 rightGazeDir = MU_AnglesToDirection(eyeGaze.rightEyePitch, eyeGaze.rightEyeYaw);

    // Calculate the average gaze direction (center of both eyes' focus)
    MU_Vector3 avgGazeDir = {
        (leftGazeDir.x + rightGazeDir.x) * 0.5f,
        (leftGazeDir.y + rightGazeDir.y) * 0.5f,
        (leftGazeDir.z + rightGazeDir.z) * 0.5f
    };
    avgGazeDir = MU_VectorNormalize(avgGazeDir);

    // Convert average direction back to pitch/yaw
    MU_DirectionToAngles(avgGazeDir, &result.pitch, &result.yaw);

    // Calculate true 3D convergence angle between the two gaze vectors
    float dotProduct = MU_VectorDot(leftGazeDir, rightGazeDir);
    // Clamp to prevent numerical errors with acos
    dotProduct = (dotProduct < -1.0f) ? -1.0f : (dotProduct > 1.0f) ? 1.0f : dotProduct;
    float convergenceAngle = MU_RadToDeg(acos(dotProduct));

    // Clamp convergence angle to our expected range
    if (convergenceAngle > params.maxConvergenceAngle) convergenceAngle = params.maxConvergenceAngle;

    // Convert to normalized convergence value (0.0 to 1.0)
    result.convergence = convergenceAngle / params.maxConvergenceAngle;

    return result;
}

/* Convert unified gaze back to individual eye gaze (inverse function) */
MU_EyeGaze MU_ConvertFromUnifiedGaze(MU_UnifiedGaze unifiedGaze, MU_ConvergenceParams params)
{
    MU_EyeGaze result;

    // Convert unified pitch/yaw to central gaze direction
    MU_Vector3 centralGazeDir = MU_AnglesToDirection(unifiedGaze.pitch, unifiedGaze.yaw);

    // Calculate the convergence angle from normalized value
    float convergenceAngle = unifiedGaze.convergence * params.maxConvergenceAngle;
    float halfConvergenceAngle = convergenceAngle * 0.5f;

    // Create rotation matrices to rotate the central gaze direction by +/- half convergence
    // We need to rotate around an axis perpendicular to both the central gaze and the eye separation vector

    // Eye separation vector (left to right) in the HMD coordinate system
    MU_Vector3 eyeSeparation = { 1.0f, 0.0f, 0.0f }; // X-axis (left to right)

    // Calculate rotation axis (perpendicular to both central gaze and eye separation)
    MU_Vector3 rotationAxis = MU_VectorCross(centralGazeDir, eyeSeparation);
    rotationAxis = MU_VectorNormalize(rotationAxis);

    // Create rotation matrices for left and right eye convergence
    float halfConvergenceRad = MU_DegToRad(halfConvergenceAngle);

    // For left eye: rotate inward (toward center)
    // For right eye: rotate inward (toward center) 
    // The rotation direction depends on the cross product result

    // Calculate the angle each eye needs to rotate inward
    // This is a simplified geometric approach
    float eyeRotationAngle = halfConvergenceAngle;

    // Rotate central direction by +/- eye rotation angle around the vertical axis
    float centralYaw = unifiedGaze.yaw;
    float leftEyeYaw = centralYaw + eyeRotationAngle;   // Left eye looks more right
    float rightEyeYaw = centralYaw - eyeRotationAngle;  // Right eye looks more left

    // Pitch remains the same for both eyes (no vertical disparity)
    result.leftEyePitch = unifiedGaze.pitch;
    result.rightEyePitch = unifiedGaze.pitch;
    result.leftEyeYaw = leftEyeYaw;
    result.rightEyeYaw = rightEyeYaw;

    return result;
}