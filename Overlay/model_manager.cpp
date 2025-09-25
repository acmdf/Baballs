#include "model_manager.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

// Basic vertex shader for 3D models
const char* vertexShaderSource = R"(
#version 120
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aTexCoord;

uniform mat4 uMVPMatrix;
uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

varying vec3 vNormal;
varying vec3 vWorldPos;
varying vec2 vTexCoord;

void main() {
    vec4 worldPos = uModelMatrix * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModelMatrix) * aNormal;
    vTexCoord = aTexCoord;
    
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
}
)";

// Basic fragment shader for 3D models
const char* fragmentShaderSource = R"(
#version 120
varying vec3 vNormal;
varying vec3 vWorldPos;
varying vec2 vTexCoord;

uniform vec3 uLightPos;
uniform vec3 uLightColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vWorldPos);
    
    // Simple diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Ambient lighting
    vec3 ambient = vec3(0.3, 0.3, 0.3);
    
    vec3 result = ambient + diffuse;
    gl_FragColor = vec4(result, 1.0);
}
)";

ModelManager::ModelManager()
    : m_hWnd(NULL)
    , m_hDC(NULL)
    , m_hRC(NULL)
    , m_ownsContext(false)
    , m_shaderProgram(0)
    , m_vertexShader(0)
    , m_fragmentShader(0)
    , m_mvpMatrixLocation(-1)
    , m_modelMatrixLocation(-1)
    , m_viewMatrixLocation(-1)
    , m_projectionMatrixLocation(-1)
    , m_lightPosLocation(-1)
    , m_lightColorLocation(-1)
    , m_cameraPosition({ 0.0f, 0.0f, 5.0f })
    , m_cameraTarget({ 0.0f, 0.0f, 0.0f })
    , m_cameraUp({ 0.0f, 1.0f, 0.0f })
    , m_fov(45.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(100.0f) {
}

ModelManager::~ModelManager() {
    Shutdown();
}

bool ModelManager::Initialize() {
    if (!InitializeOpenGL()) {
        printf("Failed to initialize OpenGL for ModelManager\n");
        return false;
    }

    m_ownsContext = true;

    if (!InitializeShaders()) {
        printf("Failed to initialize shaders for ModelManager\n");
        CleanupOpenGL();
        return false;
    }

    printf("ModelManager initialized successfully\n");
    return true;
}

bool ModelManager::InitializeWithSharedContext(HDC hDC, HGLRC hRC) {
    if (!hDC || !hRC) {
        printf("Invalid OpenGL context provided to ModelManager\n");
        return false;
    }

    m_hDC = hDC;
    m_hRC = hRC;
    m_ownsContext = false;

    // Make context current
    if (!wglMakeCurrent(m_hDC, m_hRC)) {
        printf("Failed to make shared OpenGL context current\n");
        return false;
    }

    if (!InitializeShaders()) {
        printf("Failed to initialize shaders for ModelManager\n");
        return false;
    }

    printf("ModelManager initialized with shared OpenGL context\n");
    return true;
}

bool ModelManager::InitializeOpenGL() {
    // Create a dummy window for OpenGL context
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "ModelManagerDummyClass";
    if (!RegisterClass(&wc)) {
        printf("Failed to register window class for ModelManager\n");
        return false;
    }

    m_hWnd = CreateWindow("ModelManagerDummyClass", "Dummy OpenGL Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!m_hWnd) {
        printf("Failed to create dummy window for ModelManager\n");
        return false;
    }

    m_hDC = GetDC(m_hWnd);
    if (!m_hDC) {
        printf("Failed to get device context for ModelManager\n");
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
    if (!pixelFormat) {
        printf("Failed to choose pixel format for ModelManager\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }

    if (!SetPixelFormat(m_hDC, pixelFormat, &pfd)) {
        printf("Failed to set pixel format for ModelManager\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }

    m_hRC = wglCreateContext(m_hDC);
    if (!m_hRC) {
        printf("Failed to create OpenGL rendering context for ModelManager\n");
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }

    if (!wglMakeCurrent(m_hDC, m_hRC)) {
        printf("Failed to make OpenGL context current for ModelManager\n");
        wglDeleteContext(m_hRC);
        ReleaseDC(m_hWnd, m_hDC);
        DestroyWindow(m_hWnd);
        m_hRC = NULL;
        m_hDC = NULL;
        m_hWnd = NULL;
        return false;
    }

    return true;
}

bool ModelManager::InitializeShaders() {
    // Make sure context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Compile vertex shader
    m_vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (m_vertexShader == 0) {
        printf("Failed to compile vertex shader\n");
        return false;
    }

    // Compile fragment shader
    m_fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (m_fragmentShader == 0) {
        printf("Failed to compile fragment shader\n");
        glDeleteShader(m_vertexShader);
        m_vertexShader = 0;
        return false;
    }

    // Link shader program
    m_shaderProgram = LinkShaderProgram(m_vertexShader, m_fragmentShader);
    if (m_shaderProgram == 0) {
        printf("Failed to link shader program\n");
        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
        m_vertexShader = 0;
        m_fragmentShader = 0;
        return false;
    }

    // Get uniform locations
    m_mvpMatrixLocation = glGetUniformLocation(m_shaderProgram, "uMVPMatrix");
    m_modelMatrixLocation = glGetUniformLocation(m_shaderProgram, "uModelMatrix");
    m_viewMatrixLocation = glGetUniformLocation(m_shaderProgram, "uViewMatrix");
    m_projectionMatrixLocation = glGetUniformLocation(m_shaderProgram, "uProjectionMatrix");
    m_lightPosLocation = glGetUniformLocation(m_shaderProgram, "uLightPos");
    m_lightColorLocation = glGetUniformLocation(m_shaderProgram, "uLightColor");

    printf("Shaders compiled and linked successfully\n");
    return true;
}

GLuint ModelManager::CompileShader(GLenum shaderType, const char* source) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("Shader compilation failed: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ModelManager::LinkShaderProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Shader program linking failed: %s\n", infoLog);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void ModelManager::Shutdown() {
    // Clean up models
    for (auto& model : m_models) {
        if (model->overlayHandle != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->DestroyOverlay(model->overlayHandle);
        }

        if (model->textureId != 0) {
            glDeleteTextures(1, &model->textureId);
        }

        if (model->framebuffer != 0) {
            glDeleteFramebuffers(1, &model->framebuffer);
        }

        if (model->depthBuffer != 0) {
            glDeleteRenderbuffers(1, &model->depthBuffer);
        }

        if (model->vertexBuffer != 0) {
            glDeleteBuffers(1, &model->vertexBuffer);
        }

        if (model->indexBuffer != 0) {
            glDeleteBuffers(1, &model->indexBuffer);
        }

        if (model->normalBuffer != 0) {
            glDeleteBuffers(1, &model->normalBuffer);
        }

        if (model->uvBuffer != 0) {
            glDeleteBuffers(1, &model->uvBuffer);
        }
    }
    m_models.clear();

    // Clean up shaders
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    if (m_vertexShader != 0) {
        glDeleteShader(m_vertexShader);
        m_vertexShader = 0;
    }

    if (m_fragmentShader != 0) {
        glDeleteShader(m_fragmentShader);
        m_fragmentShader = 0;
    }

    // Clean up OpenGL context if we own it
    if (m_ownsContext) {
        CleanupOpenGL();
    }
}

void ModelManager::CleanupOpenGL() {
    if (m_hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(m_hRC);
        m_hRC = NULL;
    }

    if (m_hDC && m_hWnd) {
        ReleaseDC(m_hWnd, m_hDC);
        m_hDC = NULL;
    }

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

int ModelManager::CreateModel(const std::string& name) {
    auto model = std::make_unique<Model3D>();
    model->name = name;

    // Create OpenVR overlay
    vr::VROverlayError overlayError = vr::VROverlay()->CreateOverlay(
        (name + "_model").c_str(),
        name.c_str(),
        &model->overlayHandle);

    if (overlayError != vr::VROverlayError_None) {
        printf("Failed to create overlay for model '%s': %s\n",
               name.c_str(), vr::VROverlay()->GetOverlayErrorNameFromEnum(overlayError));
        return -1;
    }

    // Set up initial overlay properties
    vr::VROverlay()->SetOverlayWidthInMeters(model->overlayHandle, 1.0f);
    vr::VROverlay()->SetOverlayAlpha(model->overlayHandle, 1.0f);

    // Position overlay in world space
    vr::HmdMatrix34_t worldTransform = { 0 };
    worldTransform.m[0][0] = 1.0f;
    worldTransform.m[1][1] = 1.0f;
    worldTransform.m[2][2] = 1.0f;
    worldTransform.m[0][3] = model->position.x;
    worldTransform.m[1][3] = model->position.y;
    worldTransform.m[2][3] = model->position.z;

    vr::VROverlay()->SetOverlayTransformAbsolute(
        model->overlayHandle,
        vr::TrackingUniverseStanding,
        &worldTransform);

    // Create framebuffer for rendering
    if (!CreateFramebuffer(model.get())) {
        printf("Failed to create framebuffer for model '%s'\n", name.c_str());
        vr::VROverlay()->DestroyOverlay(model->overlayHandle);
        return -1;
    }

    int modelId = static_cast<int>(m_models.size());
    m_models.push_back(std::move(model));

    printf("Created model '%s' with ID %d\n", name.c_str(), modelId);
    return modelId;
}

bool ModelManager::CreateFramebuffer(Model3D* model) {
    if (!model)
        return false;

    // Make sure context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Generate framebuffer
    glGenFramebuffers(1, &model->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, model->framebuffer);

    // Generate texture for color attachment
    glGenTextures(1, &model->textureId);
    glBindTexture(GL_TEXTURE_2D, model->textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, model->textureWidth, model->textureHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, model->textureId, 0);

    // Generate depth buffer
    glGenRenderbuffers(1, &model->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, model->depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, model->textureWidth, model->textureHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, model->depthBuffer);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer is not complete for model '%s'\n", model->name.c_str());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

ModelManager::Model3D* ModelManager::GetModel(int modelId) {
    if (modelId < 0 || modelId >= static_cast<int>(m_models.size())) {
        return nullptr;
    }
    return m_models[modelId].get();
}

int ModelManager::GetModelCount() const {
    return static_cast<int>(m_models.size());
}

void ModelManager::SetModelPosition(int modelId, float x, float y, float z) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->position = { x, y, z };

    // Update overlay transform
    vr::HmdMatrix34_t worldTransform = { 0 };
    worldTransform.m[0][0] = 1.0f;
    worldTransform.m[1][1] = 1.0f;
    worldTransform.m[2][2] = 1.0f;
    worldTransform.m[0][3] = x;
    worldTransform.m[1][3] = y;
    worldTransform.m[2][3] = z;

    vr::VROverlay()->SetOverlayTransformAbsolute(
        model->overlayHandle,
        vr::TrackingUniverseStanding,
        &worldTransform);
}

void ModelManager::SetModelRotation(int modelId, float x, float y, float z) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->rotation = { x, y, z };
}

void ModelManager::SetModelScale(int modelId, float x, float y, float z) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->scale = { x, y, z };
}

void ModelManager::SetCamera(const MU_Vector3& position, const MU_Vector3& target, const MU_Vector3& up) {
    m_cameraPosition = position;
    m_cameraTarget = target;
    m_cameraUp = up;
}

void ModelManager::SetCameraFOV(float fov) {
    m_fov = fov;
}

void ModelManager::SetCameraClipping(float nearPlane, float farPlane) {
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void ModelManager::ShowOverlay(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    vr::VROverlay()->ShowOverlay(model->overlayHandle);
}

void ModelManager::HideOverlay(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    vr::VROverlay()->HideOverlay(model->overlayHandle);
}

bool ModelManager::IsOverlayVisible(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return false;

    return vr::VROverlay()->IsOverlayVisible(model->overlayHandle);
}

void ModelManager::Update(float deltaTime) {
    // Make sure context is current
    wglMakeCurrent(m_hDC, m_hRC);

    for (auto& model : m_models) {
        // Update animations
        if (model->isAnimating) {
            UpdateAnimation(model.get(), deltaTime);
        }

        // Update blendshapes
        UpdateBlendShapes(model.get());

        // Render model
        RenderModel(static_cast<int>(&model - &m_models[0]));
    }
}

void ModelManager::RenderModel(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model || model->indexCount == 0)
        return;

    // Make sure context is current
    wglMakeCurrent(m_hDC, m_hRC);

    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, model->framebuffer);
    glViewport(0, 0, model->textureWidth, model->textureHeight);

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent background

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Use shader program
    glUseProgram(m_shaderProgram);

    // Create matrices
    float aspect = static_cast<float>(model->textureWidth) / static_cast<float>(model->textureHeight);
    MU_Matrix4 projectionMatrix = CreateProjectionMatrix(m_fov, aspect, m_nearPlane, m_farPlane);
    MU_Matrix4 viewMatrix = CreateViewMatrix(m_cameraPosition, m_cameraTarget, m_cameraUp);
    MU_Matrix4 modelMatrix = CreateModelMatrix(model->position, model->rotation, model->scale);
    MU_Matrix4 mvpMatrix = MU_MatrixMultiply(MU_MatrixMultiply(projectionMatrix, viewMatrix), modelMatrix);

    // Set uniforms
    if (m_mvpMatrixLocation != -1) {
        glUniformMatrix4fv(m_mvpMatrixLocation, 1, GL_FALSE, (float*)&mvpMatrix);
    }
    if (m_modelMatrixLocation != -1) {
        glUniformMatrix4fv(m_modelMatrixLocation, 1, GL_FALSE, (float*)&modelMatrix);
    }
    if (m_viewMatrixLocation != -1) {
        glUniformMatrix4fv(m_viewMatrixLocation, 1, GL_FALSE, (float*)&viewMatrix);
    }
    if (m_projectionMatrixLocation != -1) {
        glUniformMatrix4fv(m_projectionMatrixLocation, 1, GL_FALSE, (float*)&projectionMatrix);
    }
    if (m_lightPosLocation != -1) {
        glUniform3f(m_lightPosLocation, 2.0f, 2.0f, 2.0f);
    }
    if (m_lightColorLocation != -1) {
        glUniform3f(m_lightColorLocation, 1.0f, 1.0f, 1.0f);
    }

    // Bind vertex data
    if (model->vertexBuffer != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
        GLint positionLocation = glGetAttribLocation(m_shaderProgram, "aPosition");
        if (positionLocation != -1) {
            glEnableVertexAttribArray(positionLocation);
            glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        }
    }

    if (model->normalBuffer != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, model->normalBuffer);
        GLint normalLocation = glGetAttribLocation(m_shaderProgram, "aNormal");
        if (normalLocation != -1) {
            glEnableVertexAttribArray(normalLocation);
            glVertexAttribPointer(normalLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        }
    }

    if (model->uvBuffer != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, model->uvBuffer);
        GLint texCoordLocation = glGetAttribLocation(m_shaderProgram, "aTexCoord");
        if (texCoordLocation != -1) {
            glEnableVertexAttribArray(texCoordLocation);
            glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
        }
    }

    // Draw
    if (model->indexBuffer != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
        glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);
    }

    // Update overlay texture
    vr::Texture_t overlayTexture = { 0 };
    overlayTexture.handle = (void*)(uintptr_t)model->textureId;
    overlayTexture.eType = vr::TextureType_OpenGL;
    overlayTexture.eColorSpace = vr::ColorSpace_Auto;

    vr::VROverlayError texError = vr::VROverlay()->SetOverlayTexture(model->overlayHandle, &overlayTexture);
    if (texError != vr::VROverlayError_None) {
        printf("Failed to set overlay texture for model '%s': %s\n",
               model->name.c_str(), vr::VROverlay()->GetOverlayErrorNameFromEnum(texError));
    }

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Matrix helper functions
MU_Matrix4 ModelManager::CreateProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane) {
    MU_Matrix4 result = { 0 };
    float tanHalfFov = tanf(fov * M_PI / 360.0f); // fov is in degrees, convert to radians and half

    result.m[0][0] = 1.0f / (aspect * tanHalfFov);
    result.m[1][1] = 1.0f / tanHalfFov;
    result.m[2][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.m[2][3] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    result.m[3][2] = -1.0f;

    return result;
}

MU_Matrix4 ModelManager::CreateViewMatrix(const MU_Vector3& position, const MU_Vector3& target, const MU_Vector3& up) {
    MU_Vector3 forward = MU_VectorNormalize(MU_VectorSubtract(target, position));
    MU_Vector3 right = MU_VectorNormalize(MU_VectorCross(forward, up));
    MU_Vector3 realUp = MU_VectorCross(right, forward);

    MU_Matrix4 result = { 0 };
    result.m[0][0] = right.x;
    result.m[1][0] = right.y;
    result.m[2][0] = right.z;
    result.m[0][1] = realUp.x;
    result.m[1][1] = realUp.y;
    result.m[2][1] = realUp.z;
    result.m[0][2] = -forward.x;
    result.m[1][2] = -forward.y;
    result.m[2][2] = -forward.z;
    result.m[3][0] = -MU_VectorDot(right, position);
    result.m[3][1] = -MU_VectorDot(realUp, position);
    result.m[3][2] = MU_VectorDot(forward, position);
    result.m[3][3] = 1.0f;

    return result;
}

MU_Matrix4 ModelManager::CreateModelMatrix(const MU_Vector3& position, const MU_Vector3& rotation, const MU_Vector3& scale) {
    // Create scale matrix
    MU_Matrix4 scaleMatrix = MU_MatrixIdentity();
    scaleMatrix.m[0][0] = scale.x;
    scaleMatrix.m[1][1] = scale.y;
    scaleMatrix.m[2][2] = scale.z;

    // Create rotation matrices
    float cx = cosf(rotation.x * M_PI / 180.0f);
    float sx = sinf(rotation.x * M_PI / 180.0f);
    float cy = cosf(rotation.y * M_PI / 180.0f);
    float sy = sinf(rotation.y * M_PI / 180.0f);
    float cz = cosf(rotation.z * M_PI / 180.0f);
    float sz = sinf(rotation.z * M_PI / 180.0f);

    MU_Matrix4 rotationMatrix = MU_MatrixIdentity();
    rotationMatrix.m[0][0] = cy * cz;
    rotationMatrix.m[0][1] = -cy * sz;
    rotationMatrix.m[0][2] = sy;
    rotationMatrix.m[1][0] = sx * sy * cz + cx * sz;
    rotationMatrix.m[1][1] = -sx * sy * sz + cx * cz;
    rotationMatrix.m[1][2] = -sx * cy;
    rotationMatrix.m[2][0] = -cx * sy * cz + sx * sz;
    rotationMatrix.m[2][1] = cx * sy * sz + sx * cz;
    rotationMatrix.m[2][2] = cx * cy;

    // Create translation matrix
    MU_Matrix4 translationMatrix = MU_MatrixIdentity();
    translationMatrix.m[3][0] = position.x;
    translationMatrix.m[3][1] = position.y;
    translationMatrix.m[3][2] = position.z;

    // Combine: Translation * Rotation * Scale
    return MU_MatrixMultiply(MU_MatrixMultiply(translationMatrix, rotationMatrix), scaleMatrix);
}

// Placeholder implementations for blendshape and animation functions
void ModelManager::UpdateBlendShapes(Model3D* model) {
    if (!model || model->blendShapes.empty())
        return;

    // Reset to base vertices
    model->currentVertices = model->baseVertices;

    // Apply each blendshape
    for (const auto& blendShape : model->blendShapes) {
        if (blendShape.weight > 0.0f) {
            ApplyBlendShape(model, blendShape);
        }
    }

    // Update vertex buffer
    UpdateVertexBuffer(model);
}

void ModelManager::ApplyBlendShape(Model3D* model, const BlendShape& blendShape) {
    if (model->currentVertices.size() != blendShape.targetVertices.size())
        return;

    for (size_t i = 0; i < model->currentVertices.size(); ++i) {
        MU_Vector3 delta = MU_VectorSubtract(blendShape.targetVertices[i], model->baseVertices[i]);
        delta = MU_VectorScale(delta, blendShape.weight);
        model->currentVertices[i] = MU_VectorAdd(model->currentVertices[i], delta);
    }
}

void ModelManager::UpdateVertexBuffer(Model3D* model) {
    if (!model || model->vertexBuffer == 0 || model->currentVertices.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    model->currentVertices.size() * sizeof(MU_Vector3),
                    model->currentVertices.data());
}

void ModelManager::UpdateAnimation(Model3D* model, float deltaTime) {
    // Animation implementation placeholder
    if (!model || !model->isAnimating || model->currentAnimation.empty())
        return;

    model->animationTime += deltaTime;

    // Find current animation
    AnimationClip* currentClip = nullptr;
    for (auto& clip : model->animations) {
        if (clip.name == model->currentAnimation) {
            currentClip = &clip;
            break;
        }
    }

    if (!currentClip)
        return;

    // Handle looping
    if (model->animationTime >= currentClip->duration) {
        if (currentClip->loop) {
            model->animationTime = fmodf(model->animationTime, currentClip->duration);
        } else {
            model->isAnimating = false;
            model->animationTime = currentClip->duration;
        }
    }

    // Find keyframes and interpolate
    // Implementation would go here...
}

// Stub implementations for remaining functions
bool ModelManager::AddBlendShape(int modelId, const std::string& name, const std::vector<MU_Vector3>& targetVertices) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return false;

    BlendShape blendShape;
    blendShape.name = name;
    blendShape.targetVertices = targetVertices;
    blendShape.weight = 0.0f;

    model->blendShapes.push_back(blendShape);
    return true;
}

void ModelManager::SetBlendShapeWeight(int modelId, const std::string& blendShapeName, float weight) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    for (auto& blendShape : model->blendShapes) {
        if (blendShape.name == blendShapeName) {
            blendShape.weight = std::max(0.0f, std::min(1.0f, weight));
            break;
        }
    }
}

float ModelManager::GetBlendShapeWeight(int modelId, const std::string& blendShapeName) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return 0.0f;

    for (const auto& blendShape : model->blendShapes) {
        if (blendShape.name == blendShapeName) {
            return blendShape.weight;
        }
    }
    return 0.0f;
}

void ModelManager::SetAllBlendShapeWeights(int modelId, const std::map<std::string, float>& weights) {
    for (const auto& pair : weights) {
        SetBlendShapeWeight(modelId, pair.first, pair.second);
    }
}

void ModelManager::ClearBlendShapeWeights(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    for (auto& blendShape : model->blendShapes) {
        blendShape.weight = 0.0f;
    }
}

// Additional stub implementations
int ModelManager::LoadModel(const std::string& name, const std::string& filePath) {
    // Create basic model for now
    int modelId = CreateModel(name);
    if (modelId == -1)
        return -1;

    // TODO: Implement actual model loading (OBJ, glTF, etc.)
    printf("Model loading from file not yet implemented: %s\n", filePath.c_str());

    return modelId;
}

bool ModelManager::RemoveModel(int modelId) {
    if (modelId < 0 || modelId >= static_cast<int>(m_models.size())) {
        return false;
    }

    // Clean up the model's resources
    auto& model = m_models[modelId];
    if (model->overlayHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->DestroyOverlay(model->overlayHandle);
    }
    // ... other cleanup ...

    m_models.erase(m_models.begin() + modelId);
    return true;
}

void ModelManager::SetModelWorldTransform(int modelId, const MU_Matrix4& transform) {
    // Extract position, rotation, scale from transform matrix
    // This is a simplified implementation
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->position = MU_MatrixGetPosition(transform);
    // TODO: Extract rotation and scale from matrix
}

void ModelManager::SetOverlaySize(int modelId, float sizeInMeters) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    vr::VROverlay()->SetOverlayWidthInMeters(model->overlayHandle, sizeInMeters);
}

bool ModelManager::AddAnimation(int modelId, const AnimationClip& animation) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return false;

    model->animations.push_back(animation);
    return true;
}

void ModelManager::PlayAnimation(int modelId, const std::string& animationName, bool loop) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->currentAnimation = animationName;
    model->animationTime = 0.0f;
    model->isAnimating = true;

    // Set loop flag for the animation
    for (auto& clip : model->animations) {
        if (clip.name == animationName) {
            clip.loop = loop;
            break;
        }
    }
}

void ModelManager::StopAnimation(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->isAnimating = false;
    model->animationTime = 0.0f;
}

void ModelManager::PauseAnimation(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->isAnimating = false;
}

void ModelManager::ResumeAnimation(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    if (!model->currentAnimation.empty()) {
        model->isAnimating = true;
    }
}

void ModelManager::SetAnimationTime(int modelId, float time) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return;

    model->animationTime = time;
}

float ModelManager::GetAnimationTime(int modelId) {
    Model3D* model = GetModel(modelId);
    if (!model)
        return 0.0f;

    return model->animationTime;
}

void ModelManager::RenderAllModels() {
    for (int i = 0; i < GetModelCount(); ++i) {
        RenderModel(i);
    }
}

ModelManager::AnimationKeyframe ModelManager::InterpolateKeyframes(const AnimationKeyframe& a, const AnimationKeyframe& b, float t) {
    AnimationKeyframe result;
    result.time = a.time + (b.time - a.time) * t;

    // Interpolate blend weights
    for (const auto& pair : a.blendWeights) {
        const std::string& name = pair.first;
        float weightA = pair.second;
        float weightB = 0.0f;

        auto it = b.blendWeights.find(name);
        if (it != b.blendWeights.end()) {
            weightB = it->second;
        }

        result.blendWeights[name] = weightA + (weightB - weightA) * t;
    }

    return result;
}