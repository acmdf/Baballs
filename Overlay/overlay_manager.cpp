#include "overlay_manager.h"
#include "routine.h"
#include "flags.h"
#include <cstring>
#include <cmath>
#include <string>

// Target texture constants
#define TARGET_TEXTURE_WIDTH 512
#define TARGET_TEXTURE_HEIGHT 512

#define BORDER_TEXTURE_WIDTH 512
#define BORDER_TEXTURE_HEIGHT 512

float g_AnimationProgress = 0.0f;

float OverlayManager::s_routinePitch = 0.0f;
float OverlayManager::s_routineYaw = 0.0f;
float OverlayManager::s_routineDistance = TARGET_DEFAULT_DISTANCE;
uint32_t OverlayManager::s_routineState = 0;

bool OverlayManager::s_positionInitialized = false;

//RoutineController g_routineController;

OverlayManager::OverlayManager() :
    m_ulOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_ulThumbnailHandle(vr::k_ulOverlayHandleInvalid),
    m_ulBorderOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_glTextureId(0),
    m_glBorderTextureId(0),
    m_nTextureWidth(TARGET_TEXTURE_WIDTH),
    m_nTextureHeight(TARGET_TEXTURE_HEIGHT),
    m_borderTextureWidth(BORDER_TEXTURE_WIDTH),
    m_borderTextureHeight(BORDER_TEXTURE_HEIGHT),
    m_pTextureData(nullptr),
    m_borderTextureData(nullptr),
    m_targetYawAngle(0.0f),
    m_targetPitchAngle(0.0f),
    m_isVisible(true),
    m_targetIsPreview(false),
    g_routineController(RoutineController(1.15f)),
    m_hWnd(NULL),
    m_hDC(NULL),
    m_hRC(NULL)
{
}


OverlayManager::~OverlayManager()
{
    Shutdown();
}

void OverlayManager::StartRoutine(uint32_t routine){
    g_routineController.loadRoutine(routine);
    g_routineController.reset();
}

bool OverlayManager::Initialize()
{
    //g_routineController = RoutineController(1.15f);

    g_routineController.loadRoutine(11);

    printf("Loaded routine 11, state: %u\n", g_routineController.getStateFlags());

    g_routineController.reset();

    // Initialize OpenGL first
    if (!InitializeOpenGL())
    {
        printf("Failed to initialize OpenGL\n");
        return false;
    }

    // Create the main target overlay
    vr::VROverlayError overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_target", "Peripheral Vision Target", &m_ulOverlayHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        return false;
    }
    
    printf("Overlay created with handle: %llu\n", m_ulOverlayHandle);

    // Create the border overlay
    overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_border", "Peripheral Vision Border", &m_ulBorderOverlayHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create border overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    
    printf("Border overlay created with handle: %llu\n", m_ulBorderOverlayHandle);

    // Create the thumbnail overlay
    overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_target_thumb", "Peripheral Vision Target Thumbnail", &m_ulThumbnailHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create thumbnail overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulBorderOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulBorderOverlayHandle = vr::k_ulOverlayHandleInvalid;
        return false;
    }

    // Set overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, TARGET_SIZE_METERS);
    vr::VROverlay()->SetOverlayAlpha(m_ulOverlayHandle, TARGET_OPACITY);
    
    // Set border overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulBorderOverlayHandle, TARGET_SIZE_METERS * BORDER_SIZE_RATIO);
    vr::VROverlay()->SetOverlayAlpha(m_ulBorderOverlayHandle, BORDER_OPACITY);
    
    printf("Setting overlay width to: %.2f meters\n", TARGET_SIZE_METERS);
    printf("Setting border overlay width to: %.2f meters\n", TARGET_SIZE_METERS * BORDER_SIZE_RATIO);

    // Make sure the overlays are visible when created
    if (!m_isVisible)
    {
        vr::VROverlay()->HideOverlay(m_ulOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulBorderOverlayHandle);
    }
    else
    {
        vr::VROverlayError showError = vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
        printf("Show target overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
        
        showError = vr::VROverlay()->ShowOverlay(m_ulBorderOverlayHandle);
        printf("Show border overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
    }

    // Create the target texture
    if (!CreateTargetTexture())
    {
        Shutdown();
        return false;
    }

    // Update the overlay textures
    UpdateOverlayTexture();

    // Set initial position
    Update();

    return true;
}

bool OverlayManager::InitializeOpenGL()
{
    // Create a dummy window for OpenGL context
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "OVRDummyClass";
    
    if (!RegisterClass(&wc))
    {
        printf("Failed to register window class\n");
        return false;
    }
    
    m_hWnd = CreateWindow("OVRDummyClass", "Dummy OpenGL Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!m_hWnd)
    {
        printf("Failed to create dummy window\n");
        return false;
    }
    
    m_hDC = GetDC(m_hWnd);
    if (!m_hDC)
    {
        printf("Failed to get device context\n");
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
        return false;
    }
    
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
    if (!pixelFormat)
    {
        printf("Failed to choose pixel format\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }
    
    if (!SetPixelFormat(m_hDC, pixelFormat, &pfd))
    {
        printf("Failed to set pixel format\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }
    
    m_hRC = wglCreateContext(m_hDC);
    if (!m_hRC)
    {
        printf("Failed to create OpenGL rendering context\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }
    
    if (!wglMakeCurrent(m_hDC, m_hRC))
    {
        printf("Failed to make OpenGL context current\n");
        wglDeleteContext(m_hRC);
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hRC = NULL;
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }
    
    printf("OpenGL initialized successfully\n");
    return true;
}

void OverlayManager::Shutdown()
{
    // Destroy the overlays
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }

    if (m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_ulBorderOverlayHandle);
        m_ulBorderOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }

    // Destroy the thumbnail overlay
    if (m_ulThumbnailHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_ulThumbnailHandle);
        m_ulThumbnailHandle = vr::k_ulOverlayHandleInvalid;
    }

    // Delete OpenGL textures
    if (m_glTextureId != 0)
    {
        glDeleteTextures(1, &m_glTextureId);
        m_glTextureId = 0;
    }

    if (m_glBorderTextureId != 0)
    {
        glDeleteTextures(1, &m_glBorderTextureId);
        m_glBorderTextureId = 0;
    }
    
    // Free texture data memory
    if (m_pTextureData != nullptr)
    {
        delete[] m_pTextureData;
        m_pTextureData = nullptr;
    }
    if (m_borderTextureData != nullptr)
    {
        delete[] m_borderTextureData;
        m_borderTextureData = nullptr;
    }
    
    // Clean up OpenGL
    if (m_hRC)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(m_hRC);
        m_hRC = NULL;
    }
    
    if (m_hDC && m_hWnd)
    {
        ReleaseDC(m_hWnd, m_hDC);
        m_hDC = NULL;
    }
    
    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

void OverlayManager::UpdateAnimation()
{
    //printf("Updating animation %f\n", g_AnimationProgress);
    g_AnimationProgress+= 0.010f;
    // Update the target position
    SetFixedTargetPosition(0.0f, 0.0f);
}

void OverlayManager::Update()
{
    // Only update if the overlays are valid
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && 
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        // Calculate the target position based on the current angles
        MU_Vector3 targetPosition = CalculateTargetPosition();
        
        // Update the overlay transforms
        UpdateOverlayTransform(targetPosition);
    }
}

void OverlayManager::UpdateOverlayTransform(const MU_Vector3& targetPosition)
{
    // Only update if the overlays are valid
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && 
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        // Update overlay textures if needed
        UpdateOverlayTexture();
        
        // Create a transformation matrix for the overlays
        vr::HmdMatrix34_t overlayTransform = {0};
        
        // Identity rotation (straight ahead from HMD perspective)
        overlayTransform.m[0][0] = 1.0f;
        overlayTransform.m[1][1] = 1.0f;
        overlayTransform.m[2][2] = 1.0f;
        
        

        //float angle = g_AnimationProgress;
        //float yaw = sin(angle) * 32.0f;
        //float pitch = cos(angle * 0.7f) * 32.0f; // Different frequency for more natural motion

        

        
        float yaw = 0;
        float pitch = 0;
        float distance = 0;

        if (m_targetIsPreview){
            yaw = m_targetYawAngle;
            pitch = m_targetPitchAngle;
        }else{
            if(g_routineController.isComplete()){
                OverlayManager::s_routineState = FLAG_ROUTINE_COMPLETE;
            }else{
                TargetPosition pos = g_routineController.step();
                yaw = pos.yaw;
                pitch = pos.pitch;
                distance = pos.distance; // Get the distance from TargetPosition
                OverlayManager::s_routinePitch = pitch;
                OverlayManager::s_routineYaw = yaw;
                OverlayManager::s_routineDistance = distance; // Add this static member
                OverlayManager::s_routineState = pos.state;
            }
        }

        //printf("Rendering at angles %f %f %f %f %u\n", m_targetYawAngle, m_targetPitchAngle, OverlayManager::s_routineYaw, OverlayManager::s_routinePitch, pos.state);

        // Convert yaw and pitch angles to radians
        float yawRad = yaw * (M_PI / 180.0f);
        float pitchRad = pitch * (M_PI / 180.0f);
        
        // Position relative to HMD view
        // X: right(+)/left(-) adjusted by yaw angle
        // Y: up(+)/down(-) adjusted by pitch angle
        // Z: forward(-)/backward(+) at fixed distance
        //overlayTransform.m[0][3] = sin(yawRad) * TARGET_DISTANCE_METERS;
        //overlayTransform.m[1][3] = sin(pitchRad) * TARGET_DISTANCE_METERS;
        //overlayTransform.m[2][3] = -cos(yawRad) * cos(pitchRad) * TARGET_DISTANCE_METERS; // Negative Z is forward
        // Position relative to HMD view using the specified distance
        overlayTransform.m[0][3] = sin(yawRad) * distance;
        overlayTransform.m[1][3] = sin(pitchRad) * distance;
        overlayTransform.m[2][3] = -cos(yawRad) * cos(pitchRad) * distance; // Negative Z is forward



        
        // Apply the transform to the target overlay relative to HMD
        vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
            m_ulOverlayHandle, 
            vr::k_unTrackedDeviceIndex_Hmd, 
            &overlayTransform);

        yawRad = m_targetYawAngle * (M_PI / 180.0f);
        pitchRad = m_targetPitchAngle * (M_PI / 180.0f);

        overlayTransform.m[0][3] = sin(yawRad) * TARGET_DISTANCE_METERS;
        overlayTransform.m[1][3] = sin(pitchRad) * TARGET_DISTANCE_METERS;
        overlayTransform.m[2][3] = -cos(yawRad) * cos(pitchRad) * TARGET_DISTANCE_METERS; // Negative Z is forward
        
        // Create a slightly modified transform for the border overlay
        vr::HmdMatrix34_t borderTransform = overlayTransform;
        
        // Position the border slightly behind the target
        float borderOffset = 0.001f; // 1mm behind the target
        borderTransform.m[2][3] += borderOffset;
        
        // Apply the transform to the border overlay relative to HMD
        vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
            m_ulBorderOverlayHandle, 
            vr::k_unTrackedDeviceIndex_Hmd, 
            &borderTransform);
    }
}

bool OverlayManager::CreateTargetTexture()
{
    // Make sure OpenGL context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Allocate memory for the texture data
    m_pTextureData = new uint8_t[m_nTextureWidth * m_nTextureHeight * 4];
    m_borderTextureData = new uint8_t[m_borderTextureWidth * m_borderTextureHeight * 4];
    if (!m_pTextureData || !m_borderTextureData)
    {
        printf("Failed to allocate texture data memory\n");
        return false;
    }
    
    // Clear the textures to transparent black
    memset(m_pTextureData, 0, m_nTextureWidth * m_nTextureHeight * 4);
    memset(m_borderTextureData, 0, m_borderTextureWidth * m_borderTextureHeight * 4);
    
    // Generate OpenGL textures
    glGenTextures(1, &m_glTextureId);
    glGenTextures(1, &m_glBorderTextureId);

    if (m_glTextureId == 0 || m_glBorderTextureId == 0)
    {
        printf("Failed to generate OpenGL textures\n");
        if (m_pTextureData) {
            delete[] m_pTextureData;
            m_pTextureData = nullptr;
        }
        if (m_borderTextureData) {
            delete[] m_borderTextureData;
            m_borderTextureData = nullptr;
        }
        return false;
    }
    
    // Draw the target circle and crosshairs
    int centerX = m_nTextureWidth / 2;
    int centerY = m_nTextureHeight / 2;
    int radius = m_nTextureWidth / 4;
    int thickness = TARGET_LINE_THICKNESS;
    
    // Draw the target (a circle)
    for (int y = 0; y < m_nTextureHeight; y++)
    {
        for (int x = 0; x < m_nTextureWidth; x++)
        {
            // Calculate distance from center
            int dx = x - centerX;
            int dy = y - centerY;
            float distance = sqrt(static_cast<float>(dx*dx + dy*dy));
            
            // Draw the circle with the specified thickness
            if (distance >= radius - thickness && distance <= radius + thickness)
            {
                int index = (y * m_nTextureWidth + x) * 4;
                
                // RGBA format
                m_pTextureData[index + 0] = (TARGET_COLOR >> 16) & 0xFF; // R
                m_pTextureData[index + 1] = (TARGET_COLOR >> 8) & 0xFF;  // G
                m_pTextureData[index + 2] = TARGET_COLOR & 0xFF;         // B
                m_pTextureData[index + 3] = 255;                         // A (fully opaque)
            }
        }
    }
    
    // Draw crosshairs
    for (int y = 0; y < m_nTextureHeight; y++)
    {
        for (int x = 0; x < m_nTextureWidth; x++)
        {
            // Horizontal and vertical lines through the center
            if ((abs(y - centerY) <= thickness && x >= centerX - radius && x <= centerX + radius) ||
                (abs(x - centerX) <= thickness && y >= centerY - radius && y <= centerY + radius))
            {
                int index = (y * m_nTextureWidth + x) * 4;
                
                // RGBA format
                m_pTextureData[index + 0] = (TARGET_COLOR >> 16) & 0xFF; // R
                m_pTextureData[index + 1] = (TARGET_COLOR >> 8) & 0xFF;  // G
                m_pTextureData[index + 2] = TARGET_COLOR & 0xFF;         // B
                m_pTextureData[index + 3] = 255;                         // A (fully opaque)
            }
        }
    }

    // Draw oval border
    int borderThickness = BORDER_LINE_THICKNESS;
    centerX = m_borderTextureWidth / 2;
    centerY = m_borderTextureHeight / 2;
    float diameter = (float)m_borderTextureWidth;
    float radiusX = (diameter - 1) / 2.0f;
    float radiusY = (diameter - 1) / 2.0f;

    // For pixel-perfect oval, we'll calculate distances from center
    for (int y = 0; y < m_borderTextureHeight; y++)
    {
        for (int x = 0; x < m_borderTextureWidth; x++)
        {
            // Calculate normalized distance from center
            float dx = (x - centerX) / radiusX;
            float dy = (y - centerY) / radiusY;
            float distanceSquared = dx * dx + dy * dy;
            
            // Check if pixel is within the border thickness of the oval
            // 1.0 = exact edge of oval, so we check between 1.0 and (1.0 - borderThickness/radius)
            float innerLimit = 1.0f - (borderThickness / radiusX);
            innerLimit = innerLimit * innerLimit; // Square it for comparison
            
            // Draw pixel if it's within the border region
            if (distanceSquared <= 1.0f && distanceSquared >= innerLimit)
            {
                int index = (y * m_borderTextureWidth + x) * 4;
                
                // RGBA format
                m_borderTextureData[index + 0] = (BORDER_COLOR >> 16) & 0xFF; // R
                m_borderTextureData[index + 1] = (BORDER_COLOR >> 8) & 0xFF;  // G
                m_borderTextureData[index + 2] = BORDER_COLOR & 0xFF;         // B
                m_borderTextureData[index + 3] = 255;                         // A (fully opaque)
            }
        }
    }
    
    // Bind and set up the target OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_glTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    // Upload the target texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_nTextureWidth, m_nTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);
    
    // Bind and set up the border OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_glBorderTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    // Upload the border texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_borderTextureWidth, m_borderTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_borderTextureData);

    // Check for OpenGL errors
    GLenum glError = glGetError();
    if (glError != GL_NO_ERROR)
    {
        printf("OpenGL error when creating textures: %d\n", glError);
        if (m_glTextureId != 0) {
            glDeleteTextures(1, &m_glTextureId);
            m_glTextureId = 0;
        }
        if (m_glBorderTextureId != 0) {
            glDeleteTextures(1, &m_glBorderTextureId);
            m_glBorderTextureId = 0;
        }
        if (m_pTextureData) {
            delete[] m_pTextureData;
            m_pTextureData = nullptr;
        }
        if (m_borderTextureData) {
            delete[] m_borderTextureData;
            m_borderTextureData = nullptr;
        }
        return false;
    }
    
    printf("Textures created successfully with IDs: Target=%u, Border=%u\n", m_glTextureId, m_glBorderTextureId);
    return true;
}

void OverlayManager::UpdateOverlayTexture()
{
    // Make sure OpenGL context is current
    wglMakeCurrent(m_hDC, m_hRC);
    
    // Only update if the textures exist
    if (m_glTextureId != 0 && m_glBorderTextureId != 0)
    {
        // Set up the target texture for the main overlay
        vr::Texture_t targetTexture = {0};
        targetTexture.handle = (void*)(uintptr_t)m_glTextureId;
        targetTexture.eType = vr::TextureType_OpenGL;
        targetTexture.eColorSpace = vr::ColorSpace_Auto;
        
        // Set the target overlay texture
        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulOverlayHandle, &targetTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set target overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
        
        // Also set the thumbnail texture
        vr::VROverlay()->SetOverlayTexture(m_ulThumbnailHandle, &targetTexture);
        
        // Set up the border texture for the border overlay
        vr::Texture_t borderTexture = {0};
        borderTexture.handle = (void*)(uintptr_t)m_glBorderTextureId;
        borderTexture.eType = vr::TextureType_OpenGL;
        borderTexture.eColorSpace = vr::ColorSpace_Auto;
        
        // Set the border overlay texture
        texError = vr::VROverlay()->SetOverlayTexture(m_ulBorderOverlayHandle, &borderTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set border overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }
}

vr::HmdMatrix34_t OverlayManager::GetHmdPose() const
{
    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);
    
    return trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
}

// Add this function to overlay_manager.cpp
void OverlayManager::ResetFixedTargetPosition()
{
    // Simply reset the static initialization flag
    s_positionInitialized = false;
}

// Replace the CalculateTargetPosition method with this fixed position approach
MU_Vector3 OverlayManager::CalculateTargetPosition() const
{
    // Fixed position in world space based on initial HMD position and target angles
    static MU_Vector3 fixedTargetPosition = {0.0f, 0.0f, 0.0f};
    
    // Initialize the fixed position once
    if (!s_positionInitialized)
    {
        // Get the HMD pose
        vr::HmdMatrix34_t hmdPose = GetHmdPose();
        
        // Convert SteamVR matrix to our matrix format
        MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);
        
        // Extract the HMD position
        MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);
        
        // Forward direction is the negative Z axis
        MU_Vector3 forward = {-hmdMatrix.m[0][2], -hmdMatrix.m[1][2], -hmdMatrix.m[2][2]};
        forward = MU_VectorNormalize(forward);
        
        // Position the target directly in front of the user at the specified distance
        // Ignore the target yaw/pitch angles for initial positioning
        fixedTargetPosition.x = hmdPosition.x + forward.x * TARGET_DISTANCE_METERS;
        fixedTargetPosition.y = hmdPosition.y + forward.y * TARGET_DISTANCE_METERS;
        fixedTargetPosition.z = hmdPosition.z + forward.z * TARGET_DISTANCE_METERS;
        
        s_positionInitialized = true;
        //printf("Fixed target position set at: %.2f, %.2f, %.2f\n", 
        //       fixedTargetPosition.x, fixedTargetPosition.y, fixedTargetPosition.z);
    }
    
    return fixedTargetPosition;
}

// Add this method to set a new fixed position
void OverlayManager::SetFixedTargetPosition(float yawAngle, float pitchAngle)
{
    m_targetYawAngle = yawAngle;
    m_targetPitchAngle = pitchAngle;
    
    // Reset the fixed position calculation
    ResetFixedTargetPosition();
    
    // Force recalculation of the fixed position
    CalculateTargetPosition();
    Update();
}

void OverlayManager::SetPreviewTargetPosition(float yawAngle, float pitchAngle)
{
    m_targetYawAngle = yawAngle;
    m_targetPitchAngle = pitchAngle;
    m_targetIsPreview = true;
    
    /*// Reset the fixed position calculation
    ResetFixedTargetPosition();
    
    // Force recalculation of the fixed position
    CalculateTargetPosition();
    Update();*/
}

// Add this method to calculate the current viewing angle
// In overlay_manager.cpp, implement the enhanced function:
OverlayManager::ViewingAngles OverlayManager::CalculateCurrentViewingAngle() const
{
    ViewingAngles result = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Get the HMD pose
    vr::HmdMatrix34_t hmdPose = GetHmdPose();
    
    // Convert SteamVR matrix to our matrix format
    MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);
    
    // Extract the HMD position and orientation vectors
    MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);
    MU_Vector3 hmdForward = {-hmdMatrix.m[0][2], -hmdMatrix.m[1][2], -hmdMatrix.m[2][2]};
    MU_Vector3 hmdRight = {hmdMatrix.m[0][0], hmdMatrix.m[1][0], hmdMatrix.m[2][0]};
    MU_Vector3 hmdUp = {hmdMatrix.m[0][1], hmdMatrix.m[1][1], hmdMatrix.m[2][1]};
    
    // Normalize the direction vectors
    hmdForward = MU_VectorNormalize(hmdForward);
    hmdRight = MU_VectorNormalize(hmdRight);
    hmdUp = MU_VectorNormalize(hmdUp);
    
    // Get the fixed target position
    MU_Vector3 targetPosition = CalculateTargetPosition();
    // Calculate vector from HMD to target
    MU_Vector3 toTarget = {
        targetPosition.x - hmdPosition.x,
        targetPosition.y - hmdPosition.y,
        targetPosition.z - hmdPosition.z
    };
    float distanceToTarget = MU_VectorLength(toTarget);
    toTarget = MU_VectorNormalize(toTarget);
    
    // Calculate total angle between HMD forward and target direction
    float dotProduct = MU_VectorDot(hmdForward, toTarget);
    dotProduct = (dotProduct < -1.0f) ? -1.0f : (dotProduct > 1.0f) ? 1.0f : dotProduct;
    result.total = MU_RadToDeg(static_cast<float>(acos(dotProduct)));
    
    // Calculate yaw component (horizontal angle)
    // Project both vectors onto the horizontal plane (XZ)
    MU_Vector3 hmdForwardXZ = {hmdForward.x, 0.0f, hmdForward.z};
    MU_Vector3 toTargetXZ = {toTarget.x, 0.0f, toTarget.z};
    
    hmdForwardXZ = MU_VectorNormalize(hmdForwardXZ);
    toTargetXZ = MU_VectorNormalize(toTargetXZ);
    
    float dotYaw = MU_VectorDot(hmdForwardXZ, toTargetXZ);
    dotYaw = (dotYaw < -1.0f) ? -1.0f : (dotYaw > 1.0f) ? 1.0f : dotYaw;
    float yawAngle = MU_RadToDeg(static_cast<float>(acos(dotYaw)));
    
    // Determine left/right direction using cross product
    MU_Vector3 crossYaw = MU_VectorCross(hmdForwardXZ, toTargetXZ);
    if (crossYaw.y < 0.0f) {
        yawAngle = -yawAngle; // Target is to the left
    }
    result.yaw = yawAngle;
    
    // Calculate pitch component (vertical angle)
    // Get the angle between the target and the horizontal plane
    float targetPitch = MU_RadToDeg(static_cast<float>(asin(toTarget.y)));
    
    // Get the angle between the HMD forward and the horizontal plane
    float hmdPitch = MU_RadToDeg(static_cast<float>(asin(hmdForward.y)));
    
    // The pitch difference is what we want
    result.pitch = targetPitch - hmdPitch;
    
    // Calculate roll component
    // Project the target onto a plane perpendicular to HMD forward
    MU_Vector3 projectedTarget = toTarget;
    float forwardDot = MU_VectorDot(toTarget, hmdForward);
    projectedTarget.x -= hmdForward.x * forwardDot;
    projectedTarget.y -= hmdForward.y * forwardDot;
    projectedTarget.z -= hmdForward.z * forwardDot;
    projectedTarget = MU_VectorNormalize(projectedTarget);
    
    // Calculate the angle between the projected target and HMD up
    float dotRoll = MU_VectorDot(projectedTarget, hmdUp);
    dotRoll = (dotRoll < -1.0f) ? -1.0f : (dotRoll > 1.0f) ? 1.0f : dotRoll;
    float rollAngle = MU_RadToDeg(static_cast<float>(acos(dotRoll)));
    
    // Determine clockwise/counterclockwise using cross product
    MU_Vector3 expectedRight = MU_VectorCross(hmdForward, hmdUp);
    float rightDot = MU_VectorDot(projectedTarget, expectedRight);
    if (rightDot < 0.0f) {
        rollAngle = -rollAngle; // Roll is counterclockwise
    }
    result.roll = rollAngle;
    
    return result;
}

float OverlayManager::GetCurrentYawAngle() const
{
    return m_targetYawAngle;
}

float OverlayManager::GetCurrentPitchAngle() const
{
    return m_targetPitchAngle;
}

void OverlayManager::ResetTargetPosition()
{
    m_targetYawAngle = 0.0f;
    m_targetPitchAngle = 0.0f;
    
    // Reset the fixed position
    ResetFixedTargetPosition();
    Update();
}

bool OverlayManager::IsOverlayVisible() const
{
    return m_isVisible;
}

void OverlayManager::ToggleOverlayVisibility()
{
    m_isVisible = !m_isVisible;
    
    if (m_isVisible)
    {
        vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
        vr::VROverlay()->ShowOverlay(m_ulBorderOverlayHandle);
    }
    else
    {
        vr::VROverlay()->HideOverlay(m_ulOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulBorderOverlayHandle);
    }
}

vr::VROverlayHandle_t OverlayManager::GetOverlayHandle() const
{
    return m_ulOverlayHandle;
}