#include "overlay_manager.h"
#include "routine.h"
#include "flags.h"
#include "video_player.h"
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Video playback settings
#define VIDEO_PLAYBACK_RATE 30.0f  // Adjustable FPS for video playback
#define VIDEO_TEXTURE_WIDTH 854
#define VIDEO_TEXTURE_HEIGHT 480

// Target texture constants
#define TARGET_TEXTURE_WIDTH 512
#define TARGET_TEXTURE_HEIGHT 512

#define BORDER_TEXTURE_WIDTH 512
#define BORDER_TEXTURE_HEIGHT 512

// Text overlay constants
#define TEXT_TEXTURE_WIDTH 1920
#define TEXT_TEXTURE_HEIGHT 1080

float g_AnimationProgress = 0.0f;
static float g_VideoFrameTimer = 0.0f;
static uint64_t g_LastFrameTime = 0;
static VideoPlayer g_VideoPlayer;
static bool g_VideoEnabled = false;
static std::unique_ptr<FrameData> g_CurrentFrame = nullptr;

float OverlayManager::s_routinePitch = 0.0f;
float OverlayManager::s_routineYaw = 0.0f;
float OverlayManager::s_routineDistance = TARGET_DEFAULT_DISTANCE;
float OverlayManager::s_routineFadeProgress = 0.0f;
uint32_t OverlayManager::s_routineState = 0;
bool OverlayManager::s_routineSampleWritten = false;

bool OverlayManager::s_positionInitialized = false;

OverlayManager::OverlayManager() :
    m_ulOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_ulThumbnailHandle(vr::k_ulOverlayHandleInvalid),
    m_ulBorderOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_ulTextOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_ulVideoOverlayHandle(vr::k_ulOverlayHandleInvalid),
    m_glTextureId(0),
    m_glBorderTextureId(0),
    m_glTextTextureId(0),
    m_glVideoTextureId(0),
    m_nTextureWidth(TARGET_TEXTURE_WIDTH),
    m_nTextureHeight(TARGET_TEXTURE_HEIGHT),
    m_borderTextureWidth(BORDER_TEXTURE_WIDTH),
    m_borderTextureHeight(BORDER_TEXTURE_HEIGHT),
    m_textTextureWidth(TEXT_TEXTURE_WIDTH),
    m_textTextureHeight(TEXT_TEXTURE_HEIGHT),
    m_videoTextureWidth(VIDEO_TEXTURE_WIDTH),
    m_videoTextureHeight(VIDEO_TEXTURE_HEIGHT),
    m_pTextureData(nullptr),
    m_borderTextureData(nullptr),
    m_textTextureData(nullptr),
    m_videoTextureData(nullptr),
    m_targetYawAngle(0.0f),
    m_targetPitchAngle(0.0f),
    m_isVisible(true),
    m_targetIsPreview(false),
    m_isFixedPositionMode(false),
    g_routineController(RoutineController(1.15f)),
    m_hWnd(NULL),
    m_hDC(NULL),
    m_hRC(NULL),
    m_showText(true),
    m_displayText("TEST TEST TEST"),
    m_fontBuffer(nullptr),
    m_fontSize(48.0f)  // Default font size
{
    // Initialize font struct to zero
    memset(&m_font, 0, sizeof(m_font));
}

OverlayManager::~OverlayManager()
{
    // Clear glyph cache
    for (auto& pair : m_glyphCache) {
        free(pair.second.bitmap);
    }
    m_glyphCache.clear();
    // Free font buffer
    if (m_fontBuffer) {
        delete[] m_fontBuffer;
    }
    Shutdown();
}

void OverlayManager::StartRoutine(uint32_t routine) {

    /*switch(routine){
        case 0:
        default:
            RoutineController::m_fixedStageDuration = 1.0; // 1 minute
            break;
        case 1:
            RoutineController::m_fixedStageDuration = 2.0; // 2 minutes
            break;
        case 2:
            RoutineController::m_fixedStageDuration = 3.0; // 3 minutes
            break;
        case 3:
            RoutineController::m_fixedStageDuration = 4.0; // 4 minutes
            break;

    }*/


    RoutineController::m_fixedStageDuration = 0.5 + routine;

    g_routineController.loadRoutine(routine);
    g_routineController.reset();
    // Reset timing variables to start calibration from beginning
    RoutineController::m_globalAdvancedTime = 0.0;
    RoutineController::m_stageStartTime = 0.0;
    RoutineController::m_routineStage = 0;
}

bool OverlayManager::Initialize()
{
    g_routineController.loadRoutine(11);

    printf("Loaded routine 11, state: %u\n", g_routineController.getStateFlags());

    g_routineController.reset();

    // Initialize OpenGL first
    if (!InitializeOpenGL())
    {
        printf("Failed to initialize OpenGL\n");
        return false;
    }

    // Initialize font
    if (!InitializeFont("./font.ttf", 24.0f)) {
        printf("Failed to load font for overlay\n");
        // Continue anyway, we'll use fallback rendering
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

    // Create the text overlay
    overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_text", "Peripheral Vision Text", &m_ulTextOverlayHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create text overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulBorderOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulBorderOverlayHandle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    printf("Text overlay created with handle: %llu\n", m_ulTextOverlayHandle);

    // Create the video overlay
    overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_video", "Peripheral Vision Video", &m_ulVideoOverlayHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create video overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulBorderOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulTextOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulBorderOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulTextOverlayHandle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    printf("Video overlay created with handle: %llu\n", m_ulVideoOverlayHandle);

    // Create the thumbnail overlay
    overlayError = vr::VROverlay()->CreateOverlay("peripheral_vision_target_thumb", "Peripheral Vision Target Thumbnail", &m_ulThumbnailHandle);
    if (overlayError != vr::VROverlayError_None)
    {
        printf("Failed to create thumbnail overlay with error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulBorderOverlayHandle);
        vr::VROverlay()->DestroyOverlay(m_ulTextOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulBorderOverlayHandle = vr::k_ulOverlayHandleInvalid;
        m_ulTextOverlayHandle = vr::k_ulOverlayHandleInvalid;
        return false;
    }

    // Set overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, TARGET_SIZE_METERS);
    vr::VROverlay()->SetOverlayAlpha(m_ulOverlayHandle, TARGET_OPACITY);

    // Set border overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulBorderOverlayHandle, TARGET_SIZE_METERS * BORDER_SIZE_RATIO);
    vr::VROverlay()->SetOverlayAlpha(m_ulBorderOverlayHandle, BORDER_OPACITY);

    // Set text overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulTextOverlayHandle, 1.0f); // Adjust as needed
    vr::VROverlay()->SetOverlayAlpha(m_ulTextOverlayHandle, 1.0f); // Full opacity for text

    // Set video overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulVideoOverlayHandle, 0.5f); // Smaller size
    vr::VROverlay()->SetOverlayAlpha(m_ulVideoOverlayHandle, 1.0f); // Full opacity for video

    printf("Setting overlay width to: %.2f meters\n", TARGET_SIZE_METERS);
    printf("Setting border overlay width to: %.2f meters\n", TARGET_SIZE_METERS * BORDER_SIZE_RATIO);

    // Make sure the overlays are visible when created
    if (!m_isVisible)
    {
        vr::VROverlay()->HideOverlay(m_ulOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulBorderOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulTextOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulVideoOverlayHandle);
    }
    else
    {
        vr::VROverlayError showError = vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
        printf("Show target overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
        showError = vr::VROverlay()->ShowOverlay(m_ulBorderOverlayHandle);
        printf("Show border overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
        showError = vr::VROverlay()->ShowOverlay(m_ulTextOverlayHandle);
        printf("Show text overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
        showError = vr::VROverlay()->ShowOverlay(m_ulVideoOverlayHandle);
        printf("Show video overlay result: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(showError));
    }

    // Create the target texture
    if (!CreateTargetTexture())
    {
        Shutdown();
        return false;
    }

    // Update the overlay textures
    UpdateOverlayTexture();

    // Initialize text
    SetDisplayString(m_displayText.c_str());

    // Set initial position
    Update();

    return true;
}

bool OverlayManager::InitializeFont(const char* fontPath, float fontSize) {
    // Load font file
    FILE* fontFile = fopen(fontPath, "rb");
    if (!fontFile) return false;
    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);
    m_fontBuffer = new unsigned char[size];
    fread(m_fontBuffer, 1, size, fontFile);
    fclose(fontFile);
    // Initialize font
    if (stbtt_InitFont(&m_font, m_fontBuffer, 0) == 0) {
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
        return false;
    }
    m_fontSize = fontSize;
    return true;
}

void OverlayManager::RenderText(const std::string& text, int x, int y, uint32_t color) {
    if (!m_fontBuffer || !m_textTextureData) return;

    float scale = stbtt_ScaleForPixelHeight(&m_font, m_fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&m_font, &ascent, &descent, &lineGap);
    int baseline = (int)(y - (ascent * scale));

    // Split text into lines
    std::string currentLine = "";
    size_t pos = 0;

    while (pos <= text.length()) {
        if (pos == text.length() || text[pos] == '\n') {
            // Process current line
            if (!currentLine.empty()) {
                // Calculate x position to center this specific line
                int lineWidth = MeasureLineWidth(currentLine);
                int lineX = (m_textTextureWidth - lineWidth) / 2;

                // Render this line
                RenderSingleLine(currentLine, lineX, baseline, color);
            }

            // Move to next line
            baseline -= (int)m_fontSize;
            currentLine = "";
        }
        else {
            currentLine += text[pos];
        }
        pos++;
    }
}

void OverlayManager::RenderSingleLine(const std::string& line, int x, int baseline, uint32_t color) {
    if (!m_fontBuffer || !m_textTextureData) return;

    float scale = stbtt_ScaleForPixelHeight(&m_font, m_fontSize);
    int cursorX = x;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        // Check if glyph is already cached
        auto it = m_glyphCache.find(c);
        CachedGlyph glyph;
        if (it == m_glyphCache.end()) {
            // Render the glyph
            int width, height, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(
                &m_font, 0, scale, c, &width, &height, &xoff, &yoff);
            glyph = { width, height, xoff, yoff, bitmap };
            m_glyphCache[c] = glyph;
        }
        else {
            glyph = it->second;
        }

        // Get horizontal advance
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&m_font, c, &advance, &lsb);

        // Draw the glyph
        for (int j = 0; j < glyph.height; ++j) {
            for (int k = 0; k < glyph.width; ++k) {
                int pixelX = cursorX + k + glyph.xoff;
                int pixelY = baseline - (glyph.yoff + j);

                if (pixelX >= 0 && pixelX < m_textTextureWidth &&
                    pixelY >= 0 && pixelY < m_textTextureHeight) {
                    unsigned char alpha = glyph.bitmap[j * glyph.width + k];
                    if (alpha > 0) {
                        int index = (pixelY * m_textTextureWidth + pixelX) * 4;
                        // Apply alpha blending
                        float a = alpha / 255.0f;
                        m_textTextureData[index + 0] = (uint8_t)(((color >> 16) & 0xFF) * a +
                            m_textTextureData[index + 0] * (1 - a));
                        m_textTextureData[index + 1] = (uint8_t)(((color >> 8) & 0xFF) * a +
                            m_textTextureData[index + 1] * (1 - a));
                        m_textTextureData[index + 2] = (uint8_t)((color & 0xFF) * a +
                            m_textTextureData[index + 2] * (1 - a));
                        m_textTextureData[index + 3] = 255;
                    }
                }
            }
        }
        // Advance cursor
        cursorX += (int)(advance * scale);
        // Apply kerning if there's a next character
        if (i < line.length() - 1) {
            int kern = stbtt_GetCodepointKernAdvance(&m_font, c, line[i + 1]);
            cursorX += (int)(kern * scale);
        }
    }
}

int OverlayManager::MeasureTextWidth(const std::string& text) {
    if (!m_fontBuffer) return (int)(text.length() * 8); // Fallback

    float scale = stbtt_ScaleForPixelHeight(&m_font, m_fontSize);
    int max_width = 0;
    int current_width = 0;

    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == '\n') {
            // New line - update max width and reset current
            if (current_width > max_width) {
                max_width = current_width;
            }
            current_width = 0;
            continue;
        }

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&m_font, text[i], &advance, &lsb);
        current_width += (int)(advance * scale);

        // Add kerning
        if (i < text.length() - 1 && text[i + 1] != '\n') {
            current_width += (int)(stbtt_GetCodepointKernAdvance(&m_font, text[i], text[i + 1]) * scale);
        }
    }

    // Check the last line
    if (current_width > max_width) {
        max_width = current_width;
    }

    return max_width;
}

int OverlayManager::MeasureLineWidth(const std::string& line) {
    if (!m_fontBuffer) return (int)(line.length() * 8); // Fallback

    float scale = stbtt_ScaleForPixelHeight(&m_font, m_fontSize);
    int width = 0;

    for (size_t i = 0; i < line.length(); ++i) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&m_font, line[i], &advance, &lsb);
        width += (int)(advance * scale);

        // Add kerning
        if (i < line.length() - 1) {
            width += (int)(stbtt_GetCodepointKernAdvance(&m_font, line[i], line[i + 1]) * scale);
        }
    }

    return width;
}

void OverlayManager::DrawPixel(int x, int y, uint32_t color) {
    if (!m_textTextureData || x < 0 || x >= m_textTextureWidth || y < 0 || y >= m_textTextureHeight) {
        return;
    }

    int index = (y * m_textTextureWidth + x) * 4;
    m_textTextureData[index + 0] = (color >> 16) & 0xFF; // R
    m_textTextureData[index + 1] = (color >> 8) & 0xFF;  // G
    m_textTextureData[index + 2] = color & 0xFF;         // B
    m_textTextureData[index + 3] = 255;                  // A
}

void OverlayManager::DrawLine(int x1, int y1, int x2, int y2, uint32_t color) {
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int x = x1, y = y1;

    while (true) {
        DrawPixel(x, y, color);

        if (x == x2 && y == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void OverlayManager::RenderLossGraph(const std::vector<float>& lossHistory, int x, int y, int width, int height) {
    if (lossHistory.size() < 2 || !m_textTextureData) {
        printf("DEBUG: RenderLossGraph early return - size=%zu, textData=%p\n",
            lossHistory.size(), m_textTextureData);
        return;
    }

    printf("DEBUG: RenderLossGraph called - pos(%d,%d) size(%dx%d) history=%zu\n",
        x, y, width, height, lossHistory.size());

    // Use fixed range from 0 to 0.1 since we're clamping values
    float minLoss = 0.0f;
    float maxLoss = 0.1f;
    float range = maxLoss - minLoss;

    printf("DEBUG: Using fixed loss range: 0.0 to 0.1\n");

    // Colors
    uint32_t axisColor = 0xFFFFFF;   // White axes
    uint32_t lineColor = TARGET_COLOR & 0x00FFFFFF;   // Use TARGET_COLOR from config.h (strip alpha)
    uint32_t gridColor = 0x404040;   // Dark gray grid

    // Draw background rectangle (optional, for contrast)
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            DrawPixel(px, py, 0x202020); // Dark background
        }
    }

    // Draw grid lines (subtle)
    for (int i = 1; i < 5; i++) {
        int gridY = y + (height * i) / 5;
        DrawLine(x, gridY, x + width - 1, gridY, gridColor);
    }
    for (int i = 1; i < 5; i++) {
        int gridX = x + (width * i) / 5;
        DrawLine(gridX, y, gridX, y + height - 1, gridColor);
    }

    // Draw axes
    DrawLine(x, y + height - 1, x + width - 1, y + height - 1, axisColor); // X-axis (bottom)
    DrawLine(x, y, x, y + height - 1, axisColor); // Y-axis (left)

    // Draw loss curve
    for (size_t i = 1; i < lossHistory.size(); i++) {
        // Calculate positions (invert Y so lower loss is at bottom)
        int x1 = x + static_cast<int>(((i - 1) * (width - 1)) / (lossHistory.size() - 1));
        int y1 = y + static_cast<int>(((lossHistory[i - 1] - minLoss) / range) * (height - 1));

        int x2 = x + static_cast<int>((i * (width - 1)) / (lossHistory.size() - 1));
        int y2 = y + static_cast<int>(((lossHistory[i] - minLoss) / range) * (height - 1));

        // Clamp to bounds
        y1 = max(y, min(y + height - 1, y1));
        y2 = max(y, min(y + height - 1, y2));

        // Draw line segment
        DrawLine(x1, y1, x2, y2, lineColor);

        // Draw data points as small circles (optional)
        DrawPixel(x2, y2, lineColor);
        DrawPixel(x2 - 1, y2, lineColor);
        DrawPixel(x2 + 1, y2, lineColor);
        DrawPixel(x2, y2 - 1, lineColor);
        DrawPixel(x2, y2 + 1, lineColor);
    }
}

void OverlayManager::SetDisplayString(const char* text) {
    if (text == NULL) {
        m_showText = false;
    }
    else {
        m_showText = true;
        m_displayText = text;
    }

    // Make sure OpenGL context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Update text texture if it exists
    if (m_textTextureData && m_glTextTextureId != 0) {
        // Clear the texture to transparent
        memset(m_textTextureData, 0, m_textTextureWidth * m_textTextureHeight * 4);

        // Draw text if enabled
        if (m_showText) {
            // Calculate vertical center position for the text block
            // Count lines to determine total height
            int lineCount = 1;
            for (char c : m_displayText) {
                if (c == '\n') lineCount++;
            }

            int totalTextHeight = lineCount * (int)m_fontSize;
            int startY = (m_textTextureHeight + totalTextHeight) / 2; // Center the text block vertically

            // Render the text (RenderText now handles horizontal centering per line)
            RenderText(m_displayText, 0, startY, 0xFFFFFF); // White text, x=0 because centering is handled internally
        }

        // Update the texture
        glBindTexture(GL_TEXTURE_2D, m_glTextTextureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_textTextureWidth, m_textTextureHeight,
            GL_RGBA, GL_UNSIGNED_BYTE, m_textTextureData);

        // Update the text overlay with the new texture
        vr::Texture_t textTexture = { 0 };
        textTexture.handle = (void*)(uintptr_t)m_glTextTextureId;
        textTexture.eType = vr::TextureType_OpenGL;
        textTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulTextOverlayHandle, &textTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set text overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }
}

void OverlayManager::SetDisplayStringWithGraph(const char* text, const std::vector<float>& lossHistory) {
    printf("DEBUG: SetDisplayStringWithGraph called - text=%s, lossHistory.size()=%zu\n",
        text ? "non-null" : "null", lossHistory.size());
    if (text == NULL) {
        m_showText = false;
    }
    else {
        m_showText = true;
        m_displayText = text;
    }

    // Make sure OpenGL context is current - ONLY ONCE
    wglMakeCurrent(m_hDC, m_hRC);

    // Update text texture if it exists
    if (m_textTextureData && m_glTextTextureId != 0) {
        // Clear the texture to transparent
        memset(m_textTextureData, 0, m_textTextureWidth * m_textTextureHeight * 4);

        // Draw text if enabled
        if (m_showText) {
            // Calculate vertical center position for the text block
            int lineCount = 1;
            for (char c : m_displayText) {
                if (c == '\n') lineCount++;
            }

            int totalTextHeight = lineCount * (int)m_fontSize;
            int startY = (m_textTextureHeight + totalTextHeight) / 2;

            // Render the text
            RenderText(m_displayText, 0, startY, 0xFFFFFF);
        }

        // Add loss graph if we have sufficient data
        if (lossHistory.size() > 1) {
            int graphWidth = 400;
            int graphHeight = 200;
            int graphX = (m_textTextureWidth - graphWidth) / 2;  // Center horizontally
            int graphY = 200;

            printf("DEBUG: About to render graph at (%d,%d) size(%dx%d) on texture %dx%d\n",
                graphX, graphY, graphWidth, graphHeight, m_textTextureWidth, m_textTextureHeight);

            // Render graph directly to texture data (already in proper OpenGL context)
            RenderLossGraph(lossHistory, graphX, graphY, graphWidth, graphHeight);
        }
        else {
            printf("DEBUG: Not rendering graph - lossHistory.size()=%zu\n", lossHistory.size());
        }

        // Update the texture - SINGLE UPDATE
        glBindTexture(GL_TEXTURE_2D, m_glTextTextureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_textTextureWidth, m_textTextureHeight,
            GL_RGBA, GL_UNSIGNED_BYTE, m_textTextureData);

        // Update the text overlay with the new texture
        vr::Texture_t textTexture = { 0 };
        textTexture.handle = (void*)(uintptr_t)m_glTextTextureId;
        textTexture.eType = vr::TextureType_OpenGL;
        textTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulTextOverlayHandle, &textTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set text overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }
}

bool OverlayManager::InitializeOpenGL()
{
    // Create a dummy window for OpenGL context
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"OVRDummyClass";
    if (!RegisterClass(&wc))
    {
        printf("Failed to register window class\n");
        return false;
    }
    m_hWnd = CreateWindow(L"OVRDummyClass", L"Dummy OpenGL Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
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
    PIXELFORMATDESCRIPTOR pfd = { 0 };
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

    if (m_ulTextOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_ulTextOverlayHandle);
        m_ulTextOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }
    if (m_ulVideoOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->DestroyOverlay(m_ulVideoOverlayHandle);
        m_ulVideoOverlayHandle = vr::k_ulOverlayHandleInvalid;
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

    if (m_glTextTextureId != 0)
    {
        glDeleteTextures(1, &m_glTextTextureId);
        m_glTextTextureId = 0;
    }
    if (m_glVideoTextureId != 0)
    {
        glDeleteTextures(1, &m_glVideoTextureId);
        m_glVideoTextureId = 0;
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
    if (m_textTextureData != nullptr)
    {
        delete[] m_textTextureData;
        m_textTextureData = nullptr;
    }
    if (m_videoTextureData != nullptr)
    {
        delete[] m_videoTextureData;
        m_videoTextureData = nullptr;
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
    g_AnimationProgress += 0.010f;
    // Update the target position
    SetFixedTargetPosition(0.0f, 0.0f);
}

void OverlayManager::Update()
{
    // Update video playback
    static int updateCallCount = 0;
    if (updateCallCount++ % 60 == 0) { // Debug every 60 calls
        printf("Update() called %d times, calling UpdateVideoPlayback()...\n", updateCallCount);
    }
    UpdateVideoPlayback();

    // Only update if the overlays are valid
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulTextOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulVideoOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        // Check if we need to recreate texture for dilation calibration
        static uint32_t lastDilationState = 0;
        bool isDilationStage = (s_routineState & (FLAG_DILATION_BLACK | FLAG_DILATION_WHITE | FLAG_DILATION_GRADIENT));

        // Only recreate textures for dilation-related state changes, not for every state change
        bool needsTextureRecreation = false;
        if (isDilationStage) {
            // We're in a dilation stage - check if it's different from last time
            needsTextureRecreation = (lastDilationState != s_routineState);

            // Special case: gradient stage needs continuous updates
            if (s_routineState & FLAG_DILATION_GRADIENT) {
                needsTextureRecreation = true; // Always update during gradient fade
            }
        }
        else if (lastDilationState & (FLAG_DILATION_BLACK | FLAG_DILATION_WHITE | FLAG_DILATION_GRADIENT)) {
            // We were in a dilation stage but aren't anymore - need to recreate normal textures
            needsTextureRecreation = true;
        }

        if (needsTextureRecreation) {
            // Recreate texture for dilation stages or when transitioning in/out of dilation
            printf("Recreating textures: isDilationStage=%d, lastState=0x%08X, currentState=0x%08X, stage=%d\n",
                isDilationStage, lastDilationState, s_routineState, RoutineController::m_routineStage);
            CreateTargetTexture();
            UpdateOverlayTexture();
            lastDilationState = s_routineState;
        }

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
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulTextOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        // Set overlay size based on current state
        bool isDilationStage = (s_routineState & (FLAG_DILATION_BLACK | FLAG_DILATION_WHITE | FLAG_DILATION_GRADIENT));
        if (isDilationStage) {
            // Make overlay much larger for full-screen dilation effect
            vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, 10.0f); // 10 meters wide for full FOV
            vr::VROverlay()->SetOverlayWidthInMeters(m_ulBorderOverlayHandle, 0.0f); // Hide border during dilation

            // Show text during notification stages but hide during action stages
            // Check current routine stage to determine if we should show text
            bool isNotificationStage = (RoutineController::m_routineStage == DILATION_NOTIFY_2_STAGE || RoutineController::m_routineStage == DILATION_NOTIFY_3_STAGE);
            if (isNotificationStage) {
                vr::VROverlay()->SetOverlayWidthInMeters(m_ulTextOverlayHandle, 2.0f); // Show text overlay during notifications
            }
            else {
                vr::VROverlay()->SetOverlayWidthInMeters(m_ulTextOverlayHandle, 0.0f); // Hide text during action stages
            }
        }
        else {
            // Normal crosshair size
            vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, TARGET_SIZE_METERS);
            vr::VROverlay()->SetOverlayWidthInMeters(m_ulBorderOverlayHandle, TARGET_SIZE_METERS * BORDER_SIZE_RATIO);
            vr::VROverlay()->SetOverlayWidthInMeters(m_ulTextOverlayHandle, 1.0f);

            // Show/hide video overlay based on onboarding stage
            bool showVideo = ShouldShowVideoForStage(RoutineController::m_routineStage);
            if (showVideo && g_VideoEnabled) {
                vr::VROverlay()->SetOverlayWidthInMeters(m_ulVideoOverlayHandle, 0.5f);
            }
            else {
                vr::VROverlay()->SetOverlayWidthInMeters(m_ulVideoOverlayHandle, 0.0f);
            }
        }
        // Create a transformation matrix for the overlays
        vr::HmdMatrix34_t overlayTransform = { 0 };
        // Identity rotation (straight ahead from HMD perspective)
        overlayTransform.m[0][0] = 1.0f;
        overlayTransform.m[1][1] = 1.0f;
        overlayTransform.m[2][2] = 1.0f;

        float yaw = 0;
        float pitch = 0;
        float distance = 0;

        if (m_isFixedPositionMode) {
            // In fixed position mode, use absolute world coordinates
            overlayTransform.m[0][3] = m_fixedWorldPosition.x;
            overlayTransform.m[1][3] = m_fixedWorldPosition.y;
            overlayTransform.m[2][3] = m_fixedWorldPosition.z;

            // Apply the transform to the target overlay in absolute tracking space
            vr::VROverlay()->SetOverlayTransformAbsolute(
                m_ulOverlayHandle,
                vr::TrackingUniverseStanding,
                &overlayTransform);

            // For data collection, calculate the current angles relative to HMD
            vr::HmdMatrix34_t hmdPose = GetHmdPose();
            MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);
            MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);

            // Calculate vector from HMD to fixed target
            MU_Vector3 toTarget = {
                m_fixedWorldPosition.x - hmdPosition.x,
                m_fixedWorldPosition.y - hmdPosition.y,
                m_fixedWorldPosition.z - hmdPosition.z
            };

            // Convert to pitch/yaw angles for data recording
            distance = MU_VectorLength(toTarget);
            if (distance > 0.001f) {
                toTarget = MU_VectorNormalize(toTarget);
                pitch = asin(toTarget.y) * (180.0f / M_PI);
                yaw = atan2(toTarget.x, -toTarget.z) * (180.0f / M_PI);
            }

            // Store for data collection
            OverlayManager::s_routinePitch = pitch;
            OverlayManager::s_routineYaw = yaw;
            OverlayManager::s_routineDistance = distance;
            g_routineController.step();
        }
        else {
            // Normal HMD-relative positioning
            if (m_targetIsPreview) {
                yaw = m_targetYawAngle;
                pitch = m_targetPitchAngle;
                distance = TARGET_DEFAULT_DISTANCE;
            }
            else {
                if (g_routineController.isComplete()) {
                    OverlayManager::s_routineState = FLAG_ROUTINE_COMPLETE;
                }
                else {
                    TargetPosition pos = g_routineController.step();
                    yaw = pos.yaw;
                    pitch = pos.pitch;
                    distance = pos.distance;
                    OverlayManager::s_routinePitch = pitch;
                    OverlayManager::s_routineYaw = yaw;
                    OverlayManager::s_routineDistance = distance;
                    OverlayManager::s_routineState = pos.state;
                }
            }

            // Convert yaw and pitch angles to radians
            float yawRad = yaw * (M_PI / 180.0f);
            float pitchRad = pitch * (M_PI / 180.0f);

            // Position relative to HMD view using the specified distance
            overlayTransform.m[0][3] = sin(yawRad) * distance;
            overlayTransform.m[1][3] = sin(pitchRad) * distance;
            overlayTransform.m[2][3] = -cos(yawRad) * cos(pitchRad) * distance; // Negative Z is forward

            // Apply the transform to the target overlay relative to HMD
            vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
                m_ulOverlayHandle,
                vr::k_unTrackedDeviceIndex_Hmd,
                &overlayTransform);
        }

        // Create a slightly modified transform for the border overlay
        vr::HmdMatrix34_t borderTransform = overlayTransform;
        // Position the border slightly behind the target
        float borderOffset = 0.001f; // 1mm behind the target
        borderTransform.m[2][3] += borderOffset;

        // Apply the same transform type as the main overlay
        if (m_isFixedPositionMode) {
            vr::VROverlay()->SetOverlayTransformAbsolute(
                m_ulBorderOverlayHandle,
                vr::TrackingUniverseStanding,
                &borderTransform);
        }
        else {
            vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
                m_ulBorderOverlayHandle,
                vr::k_unTrackedDeviceIndex_Hmd,
                &borderTransform);
        }

        // Position the text overlay at a fixed position in front of the user
        vr::HmdMatrix34_t textTransform = { 0 };
        textTransform.m[0][0] = 1.0f;
        textTransform.m[1][1] = 1.0f;
        textTransform.m[2][2] = 1.0f;

        // Position text at bottom of view field
        textTransform.m[0][3] = 0.0f; // Center horizontally
        textTransform.m[1][3] = -0.19f; // Bottom of view (adjust as needed)
        textTransform.m[2][3] = -0.5f; // 1 meter in front

        vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
            m_ulTextOverlayHandle,
            vr::k_unTrackedDeviceIndex_Hmd,
            &textTransform);

        // Position the video overlay at top right of view field
        vr::HmdMatrix34_t videoTransform = { 0 };
        videoTransform.m[0][0] = 1.0f;
        videoTransform.m[1][1] = 1.0f;
        videoTransform.m[2][2] = 1.0f;

        // Position video at top right of view
        videoTransform.m[0][3] = 0.0f; // Right side
        videoTransform.m[1][3] = 0.0f; // Top of view
        videoTransform.m[2][3] = -0.5f; // 1 meter in front

        vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
            m_ulVideoOverlayHandle,
            vr::k_unTrackedDeviceIndex_Hmd,
            &videoTransform);
    }
}

bool OverlayManager::CreateTargetTexture()
{
    // Make sure OpenGL context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Free existing texture data to prevent memory leaks
    if (m_pTextureData) {
        delete[] m_pTextureData;
        m_pTextureData = nullptr;
    }
    if (m_borderTextureData) {
        delete[] m_borderTextureData;
        m_borderTextureData = nullptr;
    }
    if (m_textTextureData) {
        delete[] m_textTextureData;
        m_textTextureData = nullptr;
    }

    // Allocate memory for the texture data
    m_pTextureData = new uint8_t[m_nTextureWidth * m_nTextureHeight * 4];
    m_borderTextureData = new uint8_t[m_borderTextureWidth * m_borderTextureHeight * 4];
    m_textTextureData = new uint8_t[m_textTextureWidth * m_textTextureHeight * 4];
    m_videoTextureData = new uint8_t[m_videoTextureWidth * m_videoTextureHeight * 4];

    if (!m_pTextureData || !m_borderTextureData || !m_textTextureData || !m_videoTextureData)
    {
        printf("Failed to allocate texture data memory\n");
        return false;
    }

    // Delete existing textures before generating new ones to prevent texture leaks
    if (m_glTextureId != 0) {
        glDeleteTextures(1, &m_glTextureId);
        m_glTextureId = 0;
    }
    if (m_glBorderTextureId != 0) {
        glDeleteTextures(1, &m_glBorderTextureId);
        m_glBorderTextureId = 0;
    }
    if (m_glTextTextureId != 0) {
        glDeleteTextures(1, &m_glTextTextureId);
        m_glTextTextureId = 0;
    }
    if (m_glVideoTextureId != 0) {
        glDeleteTextures(1, &m_glVideoTextureId);
        m_glVideoTextureId = 0;
    }

    // Clear any existing OpenGL errors before texture generation
    while (glGetError() != GL_NO_ERROR) {
        // Clear error queue
    }

    // Generate OpenGL textures first
    glGenTextures(1, &m_glTextureId);
    GLenum err1 = glGetError();
    glGenTextures(1, &m_glBorderTextureId);
    GLenum err2 = glGetError();
    glGenTextures(1, &m_glTextTextureId);
    GLenum err3 = glGetError();
    glGenTextures(1, &m_glVideoTextureId);
    GLenum err4 = glGetError();

    if (err1 != GL_NO_ERROR || err2 != GL_NO_ERROR || err3 != GL_NO_ERROR || err4 != GL_NO_ERROR) {
        printf("OpenGL errors during texture generation: Target=%d, Border=%d, Text=%d, Video=%d\n", err1, err2, err3, err4);
    }

    if (m_glTextureId == 0 || m_glBorderTextureId == 0 || m_glTextTextureId == 0 || m_glVideoTextureId == 0)
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
        if (m_textTextureData) {
            delete[] m_textTextureData;
            m_textTextureData = nullptr;
        }
        return false;
    }

    // Clear the textures to transparent black (or handle dilation backgrounds)
    memset(m_pTextureData, 0, m_nTextureWidth * m_nTextureHeight * 4);
    memset(m_borderTextureData, 0, m_borderTextureWidth * m_borderTextureHeight * 4);
    memset(m_textTextureData, 0, m_textTextureWidth * m_textTextureHeight * 4);
    memset(m_videoTextureData, 0, m_videoTextureWidth * m_videoTextureHeight * 4);

    // Handle dilation calibration backgrounds
    if (s_routineState & FLAG_DILATION_BLACK) {
        // Fill with fully black screen
        memset(m_pTextureData, 0, m_nTextureWidth * m_nTextureHeight * 4);
        // Make it opaque by setting alpha channel
        for (int i = 0; i < m_nTextureWidth * m_nTextureHeight; i++) {
            m_pTextureData[i * 4 + 3] = 255; // Set alpha to opaque
        }

        // Upload the black texture to OpenGL
        glBindTexture(GL_TEXTURE_2D, m_glTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_nTextureWidth, m_nTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        printf("Textures created successfully with IDs: Target=%u, Border=%u, Text=%u\n",
            m_glTextureId, m_glBorderTextureId, m_glTextTextureId);
        return true; // Skip drawing crosshair for black screen
    }
    else if (s_routineState & FLAG_DILATION_WHITE) {
        // Fill with fully white screen
        for (int i = 0; i < m_nTextureWidth * m_nTextureHeight; i++) {
            m_pTextureData[i * 4 + 0] = 255; // Red
            m_pTextureData[i * 4 + 1] = 255; // Green  
            m_pTextureData[i * 4 + 2] = 255; // Blue
            m_pTextureData[i * 4 + 3] = 255; // Alpha
        }

        // Upload the white texture to OpenGL
        glBindTexture(GL_TEXTURE_2D, m_glTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_nTextureWidth, m_nTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        printf("Textures created successfully with IDs: Target=%u, Border=%u, Text=%u\n",
            m_glTextureId, m_glBorderTextureId, m_glTextTextureId);
        return true; // Skip drawing crosshair for white screen
    }
    else if (s_routineState & FLAG_DILATION_GRADIENT) {
        // Handle gradient fade from white to black
        // The fade progress is stored in s_routineFadeProgress (0.0 = white, 1.0 = black)
        float fadeProgress = s_routineFadeProgress; // 0.0 to 1.0
        uint8_t grayValue = (uint8_t)(255.0f * (1.0f - fadeProgress)); // 255 = white, 0 = black

        for (int i = 0; i < m_nTextureWidth * m_nTextureHeight; i++) {
            m_pTextureData[i * 4 + 0] = grayValue; // Red
            m_pTextureData[i * 4 + 1] = grayValue; // Green  
            m_pTextureData[i * 4 + 2] = grayValue; // Blue
            m_pTextureData[i * 4 + 3] = 255;      // Alpha
        }

        // Upload the gradient texture to OpenGL
        glBindTexture(GL_TEXTURE_2D, m_glTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_nTextureWidth, m_nTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        printf("Textures created successfully with IDs: Target=%u, Border=%u, Text=%u\n",
            m_glTextureId, m_glBorderTextureId, m_glTextTextureId);
        return true; // Skip drawing crosshair for gradient screen
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
            float distance = sqrt(static_cast<float>(dx * dx + dy * dy));
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

    /*
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
    }*/

    // Setup target texture
    glBindTexture(GL_TEXTURE_2D, m_glTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_nTextureWidth, m_nTextureHeight,
        0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);

    // Setup border texture
    glBindTexture(GL_TEXTURE_2D, m_glBorderTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_borderTextureWidth, m_borderTextureHeight,
        0, GL_RGBA, GL_UNSIGNED_BYTE, m_borderTextureData);

    // Setup text texture
    glBindTexture(GL_TEXTURE_2D, m_glTextTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_textTextureWidth, m_textTextureHeight,
        0, GL_RGBA, GL_UNSIGNED_BYTE, m_textTextureData);

    // Setup video texture
    glBindTexture(GL_TEXTURE_2D, m_glVideoTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_videoTextureWidth, m_videoTextureHeight,
        0, GL_RGBA, GL_UNSIGNED_BYTE, m_videoTextureData);

    // Check for OpenGL errors
    GLenum glError = glGetError();
    if (glError != GL_NO_ERROR)
    {
        printf("DEBUG: OpenGL error %d when creating textures in stage %d\n",
            glError, RoutineController::m_routineStage);
        printf("DEBUG: Texture dimensions: Target=%dx%d, Border=%dx%d, Text=%dx%d\n",
            m_nTextureWidth, m_nTextureHeight,
            m_borderTextureWidth, m_borderTextureHeight,
            m_textTextureWidth, m_textTextureHeight);
        printf("DEBUG: Texture IDs: Target=%u, Border=%u, Text=%u\n",
            m_glTextureId, m_glBorderTextureId, m_glTextTextureId);
        printf("DEBUG: Memory pointers: Target=%p, Border=%p, Text=%p\n",
            (void*)m_pTextureData, (void*)m_borderTextureData, (void*)m_textTextureData);

        // Test each texture binding/creation separately to isolate which one fails
        printf("DEBUG: Testing textures individually to isolate the problem:\n");

        // Test target texture
        glBindTexture(GL_TEXTURE_2D, m_glTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_nTextureWidth, m_nTextureHeight,
            0, GL_RGBA, GL_UNSIGNED_BYTE, m_pTextureData);
        printf("DEBUG: Target texture test: %d\n", glGetError());

        // Test border texture
        glBindTexture(GL_TEXTURE_2D, m_glBorderTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_borderTextureWidth, m_borderTextureHeight,
            0, GL_RGBA, GL_UNSIGNED_BYTE, m_borderTextureData);
        printf("DEBUG: Border texture test: %d\n", glGetError());

        // Test text texture
        glBindTexture(GL_TEXTURE_2D, m_glTextTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_textTextureWidth, m_textTextureHeight,
            0, GL_RGBA, GL_UNSIGNED_BYTE, m_textTextureData);
        printf("DEBUG: Text texture test: %d\n", glGetError());

        // Clean up resources
        if (m_glTextureId != 0) {
            glDeleteTextures(1, &m_glTextureId);
            m_glTextureId = 0;
        }
        if (m_glBorderTextureId != 0) {
            glDeleteTextures(1, &m_glBorderTextureId);
            m_glBorderTextureId = 0;
        }
        if (m_glTextTextureId != 0) {
            glDeleteTextures(1, &m_glTextTextureId);
            m_glTextTextureId = 0;
        }
        if (m_pTextureData) {
            delete[] m_pTextureData;
            m_pTextureData = nullptr;
        }
        if (m_borderTextureData) {
            delete[] m_borderTextureData;
            m_borderTextureData = nullptr;
        }
        if (m_textTextureData) {
            delete[] m_textTextureData;
            m_textTextureData = nullptr;
        }
        return false;
    }

    printf("Textures created successfully with IDs: Target=%u, Border=%u, Text=%u, Video=%u\n",
        m_glTextureId, m_glBorderTextureId, m_glTextTextureId, m_glVideoTextureId);
    return true;
}

void OverlayManager::UpdateOverlayTexture()
{
    printf("\nOverlayManager::Update()\n");
    // Make sure OpenGL context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Update target texture
    if (m_glTextureId != 0)
    {
        vr::Texture_t targetTexture = { 0 };
        targetTexture.handle = (void*)(uintptr_t)m_glTextureId;
        targetTexture.eType = vr::TextureType_OpenGL;
        targetTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulOverlayHandle, &targetTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set target overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }

        // Also set the thumbnail texture
        vr::VROverlay()->SetOverlayTexture(m_ulThumbnailHandle, &targetTexture);
    }

    // Update border texture
    if (m_glBorderTextureId != 0)
    {
        vr::Texture_t borderTexture = { 0 };
        borderTexture.handle = (void*)(uintptr_t)m_glBorderTextureId;
        borderTexture.eType = vr::TextureType_OpenGL;
        borderTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulBorderOverlayHandle, &borderTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set border overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }

    // Update text texture
    if (m_glTextTextureId != 0)
    {
        vr::Texture_t textTexture = { 0 };
        textTexture.handle = (void*)(uintptr_t)m_glTextTextureId;
        textTexture.eType = vr::TextureType_OpenGL;
        textTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulTextOverlayHandle, &textTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set text overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }

    // Update video texture if we have a current frame
    if (m_glVideoTextureId != 0 && g_CurrentFrame && g_VideoEnabled)
    {
        vr::Texture_t videoTexture = { 0 };
        videoTexture.handle = (void*)(uintptr_t)m_glVideoTextureId;
        videoTexture.eType = vr::TextureType_OpenGL;
        videoTexture.eColorSpace = vr::ColorSpace_Auto;

        vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulVideoOverlayHandle, &videoTexture);
        if (texError != vr::VROverlayError_None) {
            printf("Set video overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
        }
    }
}

vr::HmdMatrix34_t OverlayManager::GetHmdPose() const
{
    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);
    return trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
}

void OverlayManager::ResetFixedTargetPosition()
{
    // Simply reset the static initialization flag
    s_positionInitialized = false;
}

MU_Vector3 OverlayManager::CalculateTargetPosition() const
{
    // Fixed position in world space based on initial HMD position and target angles
    static MU_Vector3 fixedTargetPosition = { 0.0f, 0.0f, 0.0f };
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
        MU_Vector3 forward = { -hmdMatrix.m[0][2], -hmdMatrix.m[1][2], -hmdMatrix.m[2][2] };
        forward = MU_VectorNormalize(forward);
        // Position the target directly in front of the user at the specified distance
        // Ignore the target yaw/pitch angles for initial positioning
        fixedTargetPosition.x = hmdPosition.x + forward.x * TARGET_DISTANCE_METERS;
        fixedTargetPosition.y = hmdPosition.y + forward.y * TARGET_DISTANCE_METERS;
        fixedTargetPosition.z = hmdPosition.z + forward.z * TARGET_DISTANCE_METERS;
        s_positionInitialized = true;
    }
    return fixedTargetPosition;
}

void OverlayManager::SetFixedTargetPosition(float yawAngle, float pitchAngle)
{
    m_targetYawAngle = yawAngle;
    m_targetPitchAngle = pitchAngle;
    m_targetIsPreview = false;
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
    Update();
}

void OverlayManager::EnableFixedPositionMode(bool enable)
{
    m_isFixedPositionMode = enable;
    if (enable) {
        // When enabling fixed position mode, store the current world position
        // Get current HMD pose
        vr::HmdMatrix34_t hmdPose = GetHmdPose();
        MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);
        MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);

        // Extract the HMD's forward direction from the rotation matrix
        // In SteamVR right-handed coordinate system: +Y up, +X right, -Z forward
        MU_Vector3 forward = { -hmdMatrix.m[0][2], -hmdMatrix.m[1][2], -hmdMatrix.m[2][2] };
        forward = MU_VectorNormalize(forward);

        // Place crosshair directly in front of HMD using the actual forward direction
        float distance = TARGET_DISTANCE_METERS;
        m_fixedWorldPosition.x = hmdPosition.x + forward.x * distance;
        m_fixedWorldPosition.y = hmdPosition.y + forward.y * distance;
        m_fixedWorldPosition.z = hmdPosition.z + forward.z * distance;

        printf("Fixed position mode enabled. World position: (%.2f, %.2f, %.2f)\n",
            m_fixedWorldPosition.x, m_fixedWorldPosition.y, m_fixedWorldPosition.z);
        printf("HMD forward direction: (%.2f, %.2f, %.2f)\n",
            forward.x, forward.y, forward.z);
    }
    else {
        printf("Fixed position mode disabled\n");
    }
}

// Add this method to calculate the current viewing angle
// In overlay_manager.cpp, implement the enhanced function:
OverlayManager::ViewingAngles OverlayManager::CalculateCurrentViewingAngle() const
{
    ViewingAngles result = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Get the HMD pose
    vr::HmdMatrix34_t hmdPose = GetHmdPose();

    // Convert SteamVR matrix to our matrix format
    MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);

    // Extract the HMD position and orientation vectors
    MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);
    MU_Vector3 hmdForward = { -hmdMatrix.m[0][2], -hmdMatrix.m[1][2], -hmdMatrix.m[2][2] };
    MU_Vector3 hmdRight = { hmdMatrix.m[0][0], hmdMatrix.m[1][0], hmdMatrix.m[2][0] };
    MU_Vector3 hmdUp = { hmdMatrix.m[0][1], hmdMatrix.m[1][1], hmdMatrix.m[2][1] };

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
    MU_Vector3 hmdForwardXZ = { hmdForward.x, 0.0f, hmdForward.z };
    MU_Vector3 toTargetXZ = { toTarget.x, 0.0f, toTarget.z };

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

void OverlayManager::ShowTargetCrosshair()
{
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
        vr::VROverlay()->ShowOverlay(m_ulBorderOverlayHandle);
    }
}

void OverlayManager::HideTargetCrosshair()
{
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid &&
        m_ulBorderOverlayHandle != vr::k_ulOverlayHandleInvalid)
    {
        vr::VROverlay()->HideOverlay(m_ulOverlayHandle);
        vr::VROverlay()->HideOverlay(m_ulBorderOverlayHandle);
    }
}

void OverlayManager::SetTargetCrosshairVisibility(bool visible)
{
    if (visible)
    {
        ShowTargetCrosshair();
    }
    else
    {
        HideTargetCrosshair();
    }
}

vr::VROverlayHandle_t OverlayManager::GetOverlayHandle() const
{
    return m_ulOverlayHandle;
}

MU_EyeGaze OverlayManager::CalculateEyeGaze(MU_Vector3 leftEyeOffset, MU_Vector3 rightEyeOffset, MU_Vector3 targetPosition) const
{
    // Get current HMD pose
    vr::HmdMatrix34_t hmdPose = GetHmdPose();
    MU_Matrix4 hmdMatrix = MU_ConvertSteamVRMatrixToMatrix4(hmdPose);
    MU_Vector3 hmdPosition = MU_MatrixGetPosition(hmdMatrix);

    // Calculate eye gaze using the math utility function
    return MU_CalculateEyeGaze(hmdPosition, hmdMatrix, leftEyeOffset, rightEyeOffset, targetPosition);
}

MU_UnifiedGaze OverlayManager::CalculateUnifiedEyeGaze(MU_Vector3 leftEyeOffset, MU_Vector3 rightEyeOffset, MU_Vector3 targetPosition) const
{
    // First calculate individual eye gaze
    MU_EyeGaze eyeGaze = CalculateEyeGaze(leftEyeOffset, rightEyeOffset, targetPosition);

    // Convert to unified representation using default parameters
    MU_ConvergenceParams params = MU_CreateDefaultConvergenceParams();
    return MU_ConvertToUnifiedGaze(eyeGaze, params);
}

MU_UnifiedGaze OverlayManager::CalculateUnifiedEyeGaze(MU_Vector3 leftEyeOffset, MU_Vector3 rightEyeOffset, MU_Vector3 targetPosition, MU_ConvergenceParams params) const
{
    // First calculate individual eye gaze
    MU_EyeGaze eyeGaze = CalculateEyeGaze(leftEyeOffset, rightEyeOffset, targetPosition);

    // Convert to unified representation using provided parameters
    return MU_ConvertToUnifiedGaze(eyeGaze, params);
}

MU_Vector3 OverlayManager::GetCurrentTargetPosition() const
{
    if (m_isFixedPositionMode) {
        return m_fixedWorldPosition;
    }
    return CalculateTargetPosition();
}

void OverlayManager::LoadVideo(const std::string& filepath)
{
    if (g_VideoPlayer.LoadVideo(filepath)) {
        printf("Video loaded successfully: %s\n", filepath.c_str());
        g_VideoFrameTimer = 0.0f;
        g_CurrentFrame = nullptr;
    }
    else {
        printf("Failed to load video: %s\n", filepath.c_str());
    }
}

void OverlayManager::EnableVideo(bool enable)
{

    if (g_VideoEnabled == enable)
        return;
    g_VideoEnabled = enable;
    printf("Video enabled: %s\n", enable ? "true" : "false");

    if (!enable) {
        g_CurrentFrame = nullptr;
        // Clear video texture
        if (m_videoTextureData) {
            memset(m_videoTextureData, 0, m_videoTextureWidth * m_videoTextureHeight * 4);
            if (m_glVideoTextureId != 0) {
                wglMakeCurrent(m_hDC, m_hRC);
                glBindTexture(GL_TEXTURE_2D, m_glVideoTextureId);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_videoTextureWidth, m_videoTextureHeight,
                    GL_RGBA, GL_UNSIGNED_BYTE, m_videoTextureData);
            }
        }
        // Hide video overlay when disabled
        if (m_ulVideoOverlayHandle != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->HideOverlay(m_ulVideoOverlayHandle);
        }
    }
    else {
        // Show video overlay when enabled
        if (m_ulVideoOverlayHandle != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->ShowOverlay(m_ulVideoOverlayHandle);
        }
    }
}



void OverlayManager::UpdateVideoPlayback()
{
    static int videoUpdateCount = 0;
    if (videoUpdateCount++ % 60 == 0) { // Debug every 60 calls
        printf("UpdateVideoPlayback() called %d times, enabled=%s, loaded=%s\n",
            videoUpdateCount, g_VideoEnabled ? "true" : "false",
            g_VideoPlayer.IsLoaded() ? "true" : "false");
    }

    if (!g_VideoEnabled || !g_VideoPlayer.IsLoaded()) {
        return;
    }

    static bool firstFrame = true;
    if (firstFrame) {
        printf("Starting video playback...\n");
        firstFrame = false;
    }

    uint64_t currentTime_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now()
    ).time_since_epoch().count();

    if (g_LastFrameTime == 0) {
        g_LastFrameTime = currentTime_ns;
    }

    uint64_t delta_ns = currentTime_ns - g_LastFrameTime;

    //g_LastFrameTime = currentTime_ns;

    // Update frame timer
    g_VideoFrameTimer += (float)(static_cast<double>(delta_ns) / 1000000.0);//1.0f / VIDEO_PLAYBACK_RATE;

    static int debugCounter = 0;
    if (debugCounter++ % 60 == 0) { // Debug every 60 calls
        printf("Video timer: %.3f, target: 1.0, rate: %.1f\n", g_VideoFrameTimer, VIDEO_PLAYBACK_RATE);
    }

    // Check if we need to advance to next frame
    if (g_VideoFrameTimer >= (1000.0f / VIDEO_PLAYBACK_RATE)) {
        g_VideoFrameTimer -= (1000.0f / VIDEO_PLAYBACK_RATE);
        printf("Advancing to next video frame...\n");

        if (g_VideoPlayer.HasMoreFrames()) {
            g_CurrentFrame = g_VideoPlayer.GetNextFrame();

            if (g_CurrentFrame && m_videoTextureData && m_glVideoTextureId != 0) {
                // Convert RGB to RGBA and copy to video texture
                CopyFrameToTexture(*g_CurrentFrame);

                // Update OpenGL texture
                wglMakeCurrent(m_hDC, m_hRC);
                glBindTexture(GL_TEXTURE_2D, m_glVideoTextureId);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_videoTextureWidth, m_videoTextureHeight,
                    GL_RGBA, GL_UNSIGNED_BYTE, m_videoTextureData);

                // Update VR overlay with new texture
                vr::Texture_t videoTexture = { 0 };
                videoTexture.handle = (void*)(uintptr_t)m_glVideoTextureId;
                videoTexture.eType = vr::TextureType_OpenGL;
                videoTexture.eColorSpace = vr::ColorSpace_Auto;

                vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(m_ulVideoOverlayHandle, &videoTexture);
                if (texError != vr::VROverlayError_None) {
                    printf("Set video overlay texture error: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
                }
                else {
                    printf("Video texture updated successfully\n");
                }

                printf("Updated video texture with frame %dx%d\n", g_CurrentFrame->width, g_CurrentFrame->height);
            }
        }
        else {
            // Loop video
            g_VideoPlayer.ResetPlayback();
        }
        g_LastFrameTime = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now()
        ).time_since_epoch().count();
    }
}

void OverlayManager::CopyFrameToTexture(const FrameData& frame)
{
    if (!m_videoTextureData || !frame.pixels) {
        return;
    }

    // Clear texture first
    memset(m_videoTextureData, 0, m_videoTextureWidth * m_videoTextureHeight * 4);

    // Calculate scaling to fit video in texture while maintaining aspect ratio
    float scaleX = (float)m_videoTextureWidth / frame.width;
    float scaleY = (float)m_videoTextureHeight / frame.height;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    int scaledWidth = (int)(frame.width * scale);
    int scaledHeight = (int)(frame.height * scale);
    int offsetX = (m_videoTextureWidth - scaledWidth) / 2;
    int offsetY = (m_videoTextureHeight - scaledHeight) / 2;

    // Simple nearest neighbor scaling and RGB to RGBA conversion (flip Y to fix upside-down)
    for (int y = 0; y < scaledHeight; y++) {
        for (int x = 0; x < scaledWidth; x++) {
            int srcX = (int)((float)x / scale);
            int srcY = (int)((float)(scaledHeight - 1 - y) / scale); // Flip Y coordinate

            if (srcX < frame.width && srcY < frame.height) {
                int srcIndex = (srcY * frame.width + srcX) * frame.channels;
                int dstIndex = ((y + offsetY) * m_videoTextureWidth + (x + offsetX)) * 4;

                m_videoTextureData[dstIndex + 0] = frame.pixels[srcIndex + 0]; // R
                m_videoTextureData[dstIndex + 1] = frame.pixels[srcIndex + 1]; // G
                m_videoTextureData[dstIndex + 2] = frame.pixels[srcIndex + 2]; // B
                m_videoTextureData[dstIndex + 3] = 255; // A
            }
        }
    }
}

bool OverlayManager::ShouldShowVideoForStage(int stage)
{
    // Define which onboarding stages should show video
    // Adjust these stage numbers based on your onboarding flow
    bool shouldShow;
    switch (stage) {
    case 0: // Initial stage
    case 1: // Tutorial stage
    case 2: // Practice stage
        shouldShow = true;
        break;
    default:
        shouldShow = true; // Show for all stages for now (debugging)
        break;
    }

    printf("ShouldShowVideoForStage(%d): %s\n", stage, shouldShow ? "true" : "false");
    return shouldShow;
}