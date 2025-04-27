#pragma once

#include <openvr.h>
#include <windows.h>
#include <gl/GL.h>
#include "math_utils.h"
#include "config.h"
#include "routine.h"

class OverlayManager {

public:
    struct ViewingAngles {
        float yaw;   // Horizontal angle (left/right)
        float pitch; // Vertical angle (up/down)
        float roll;  // Tilt angle (rotation around forward axis)
        float total; // Total angular difference
    };
    OverlayManager();
    ~OverlayManager();

    // Initialize the overlay system
    bool Initialize();
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Update overlay position based on HMD position
    void Update();
    
    // Add these methods instead
    void SetFixedTargetPosition(float yawAngle, float pitchAngle);
    void SetPreviewTargetPosition(float yawAngle, float pitchAngle);
    
    // Get the current angles
    float GetCurrentYawAngle() const;
    float GetCurrentPitchAngle() const;
    
    // Reset target to center position
    void ResetTargetPosition();
    
    // Check if the overlay is visible
    bool IsOverlayVisible() const;
    
    // Toggle overlay visibility
    void ToggleOverlayVisibility();
    
    // update the tracking target animation
    void UpdateAnimation();

    // start a calibration routine
    void StartRoutine(uint32_t routine);
    
    // Get overlay handle
    vr::VROverlayHandle_t GetOverlayHandle() const;
    ViewingAngles CalculateCurrentViewingAngle() const;

    static float s_routinePitch;
    static float s_routineYaw;
    static float s_routineDistance;
    static bool s_routineSampleWritten;
    static uint32_t s_routineState;

    RoutineController g_routineController;

private:
    // OpenVR overlay handles
    vr::VROverlayHandle_t m_ulOverlayHandle;
    vr::VROverlayHandle_t m_ulThumbnailHandle;
    vr::VROverlayHandle_t m_ulBorderOverlayHandle;
    
    // OpenGL textures
    GLuint m_glTextureId;
    GLuint m_glBorderTextureId;
    
    // Texture dimensions
    int m_nTextureWidth;
    int m_nTextureHeight;
    int m_borderTextureWidth;
    int m_borderTextureHeight;
    
    // Texture data
    uint8_t* m_pTextureData;
    uint8_t* m_borderTextureData;
    
    // Current position of the target in angles (yaw, pitch)
    float m_targetYawAngle;
    float m_targetPitchAngle;

    bool m_targetIsPreview;
    
    // Flag to track if the overlay is visible
    bool m_isVisible;
    
    // Initialize OpenGL
    bool InitializeOpenGL();
    
    // Initialize the target texture
    bool CreateTargetTexture();
    
    // Update target texture if needed
    void UpdateOverlayTexture();
    
    vr::HmdMatrix34_t GetHmdPose() const;
    MU_Vector3 CalculateTargetPosition() const;
    
    // Update the overlay transform
    void UpdateOverlayTransform(const MU_Vector3& targetPosition);
    static void ResetFixedTargetPosition();
    static bool s_positionInitialized;  // Static class variable to track initialization

    // OpenGL context
    HWND m_hWnd;
    HDC m_hDC;
    HGLRC m_hRC;
};