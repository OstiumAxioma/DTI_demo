#include "../header/GLFiberRenderer.h"
#include <iostream>
#include <cmath>

namespace DTIFiberLib {

// Embedded shaders
static const char* vertexShaderSource = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aDirection;

out vec3 FragColor;

uniform mat4 uMVPMatrix;
uniform int uColorMode;

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);

    if (uColorMode == 1) {
        // Direction-based RGB coloring
        FragColor = abs(normalize(aDirection));
    } else {
        // Default solid color (red)
        FragColor = vec3(1.0, 0.0, 0.0);
    }
}
)";

static const char* fragmentShaderSource = R"(
#version 460 core
in vec3 FragColor;
out vec4 FragmentColor;

uniform float uOpacity;

void main() {
    FragmentColor = vec4(FragColor, uOpacity);
}
)";

GLFiberRenderer::GLFiberRenderer()
    : m_VAO(0)
    , m_VBO(0)
    , m_colorMode(FiberColoringMode::DIRECTION_RGB)
    , m_lineWidth(1.0f)
    , m_opacity(1.0f)
    , m_renderedTrackCount(0)
    , m_totalPointCount(0)
    , m_minX(0), m_maxX(0), m_minY(0), m_maxY(0), m_minZ(0), m_maxZ(0)
    , m_lodEnabled(false)
    , m_maxPointsPerTrack(0)
    , m_initialized(false)
    , m_needsUpload(false)
{
}

GLFiberRenderer::~GLFiberRenderer()
{
    cleanup();
}

void GLFiberRenderer::initialize()
{
    if (m_initialized) {
        return;
    }

    // Create and compile shader program
    m_shader = std::make_unique<GLShaderProgram>();
    if (!m_shader->loadFromString(vertexShaderSource, fragmentShaderSource)) {
        std::cerr << "Failed to create shader program" << std::endl;
        return;
    }

    // Generate VAO and VBO
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    // Setup VAO
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Direction attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    m_initialized = true;
    std::cout << "GLFiberRenderer initialized successfully" << std::endl;
}

void GLFiberRenderer::cleanup()
{
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    m_shader.reset();
    m_initialized = false;
}

void GLFiberRenderer::setTracks(const std::vector<FiberTrack>& tracks)
{
    m_tracks = tracks;
    m_needsUpload = true;

    // Build vertex data immediately to calculate bounding box
    // (GPU upload will happen later in render())
    buildVertexData();
}

void GLFiberRenderer::setColorMode(FiberColoringMode mode)
{
    m_colorMode = mode;
}

void GLFiberRenderer::setLineWidth(float width)
{
    m_lineWidth = width;
}

void GLFiberRenderer::setOpacity(float opacity)
{
    m_opacity = opacity;
}

void GLFiberRenderer::setLODEnabled(bool enable)
{
    m_lodEnabled = enable;
}

void GLFiberRenderer::setMaxPointsPerTrack(size_t maxPoints)
{
    m_maxPointsPerTrack = maxPoints;
}

void GLFiberRenderer::buildVertexData()
{
    m_vertexData.clear();
    m_trackStarts.clear();
    m_trackCounts.clear();
    m_totalPointCount = 0;
    m_renderedTrackCount = 0;

    for (const auto& track : m_tracks) {
        if (track.empty()) continue;

        // Record track start and count for multi-draw
        m_trackStarts.push_back(static_cast<GLint>(m_totalPointCount));
        m_trackCounts.push_back(static_cast<GLsizei>(track.size()));
        m_renderedTrackCount++;

        // Build vertex data with direction calculation
        for (size_t i = 0; i < track.size(); ++i) {
            const auto& point = track[i];

            // Calculate direction vector
            float dirX = 0.0f, dirY = 0.0f, dirZ = 0.0f;

            if (track.size() == 1) {
                dirX = dirY = dirZ = 0.5f;
            } else if (i == 0) {
                dirX = track[1].x - track[0].x;
                dirY = track[1].y - track[0].y;
                dirZ = track[1].z - track[0].z;
            } else if (i == track.size() - 1) {
                dirX = track[i].x - track[i-1].x;
                dirY = track[i].y - track[i-1].y;
                dirZ = track[i].z - track[i-1].z;
            } else {
                // Central difference
                dirX = track[i+1].x - track[i-1].x;
                dirY = track[i+1].y - track[i-1].y;
                dirZ = track[i+1].z - track[i-1].z;
            }

            // Normalize direction
            float length = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
            if (length > 0.0001f) {
                dirX /= length;
                dirY /= length;
                dirZ /= length;
            }

            // Add vertex data (position + direction)
            m_vertexData.push_back(point.x);
            m_vertexData.push_back(point.y);
            m_vertexData.push_back(point.z);
            m_vertexData.push_back(dirX);
            m_vertexData.push_back(dirY);
            m_vertexData.push_back(dirZ);

            m_totalPointCount++;
        }
    }

    // Calculate bounding box
    m_minX = 1e10; m_minY = 1e10; m_minZ = 1e10;
    m_maxX = -1e10; m_maxY = -1e10; m_maxZ = -1e10;
    for (size_t i = 0; i < m_vertexData.size(); i += 6) {
        float x = m_vertexData[i];
        float y = m_vertexData[i + 1];
        float z = m_vertexData[i + 2];
        m_minX = std::min(m_minX, x); m_maxX = std::max(m_maxX, x);
        m_minY = std::min(m_minY, y); m_maxY = std::max(m_maxY, y);
        m_minZ = std::min(m_minZ, z); m_maxZ = std::max(m_maxZ, z);
    }

    std::cout << "Built vertex data: " << m_renderedTrackCount << " tracks, "
              << m_totalPointCount << " points" << std::endl;
    std::cout << "Bounding box: X[" << m_minX << ", " << m_maxX << "] "
              << "Y[" << m_minY << ", " << m_maxY << "] "
              << "Z[" << m_minZ << ", " << m_maxZ << "]" << std::endl;
}

void GLFiberRenderer::uploadToGPU()
{
    if (!m_initialized) {
        std::cerr << "GLFiberRenderer not initialized" << std::endl;
        return;
    }

    if (m_tracks.empty()) {
        std::cout << "No tracks to upload" << std::endl;
        return;
    }

    // Build vertex data if not already built
    if (m_vertexData.empty()) {
        buildVertexData();
    }

    if (m_vertexData.empty()) {
        std::cout << "No vertex data to upload" << std::endl;
        return;
    }

    // Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 m_vertexData.size() * sizeof(float),
                 m_vertexData.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_needsUpload = false;

    std::cout << "Uploaded " << m_vertexData.size() * sizeof(float) / 1024 / 1024
              << " MB to GPU" << std::endl;
}

void GLFiberRenderer::render(const float* mvpMatrix)
{
    if (!m_initialized) {
        std::cerr << "GLFiberRenderer not initialized" << std::endl;
        return;
    }

    if (m_needsUpload) {
        uploadToGPU();
    }

    if (m_vertexData.empty()) {
        return;
    }

    // Use shader program
    m_shader->use();

    // Debug: Print MVP matrix first time
    static bool firstRender = true;
    if (firstRender) {
        std::cout << "MVP Matrix (first 4 values): "
                  << mvpMatrix[0] << ", " << mvpMatrix[1] << ", "
                  << mvpMatrix[2] << ", " << mvpMatrix[3] << std::endl;
        firstRender = false;
    }

    // Set uniforms
    m_shader->setUniformMatrix4fv("uMVPMatrix", mvpMatrix);
    m_shader->setUniform1i("uColorMode", m_colorMode == FiberColoringMode::DIRECTION_RGB ? 1 : 0);
    m_shader->setUniform1f("uOpacity", m_opacity);

    // Set line width
    glLineWidth(m_lineWidth);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Bind VAO and render
    glBindVertexArray(m_VAO);

    // Render all tracks in one call using glMultiDrawArrays
    if (!m_trackStarts.empty() && !m_trackCounts.empty()) {
        std::cout << "Rendering " << m_trackStarts.size() << " tracks with glMultiDrawArrays" << std::endl;
        glMultiDrawArrays(GL_LINE_STRIP, m_trackStarts.data(), m_trackCounts.data(), static_cast<GLsizei>(m_trackStarts.size()));

        // Check for OpenGL errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error after glMultiDrawArrays: 0x" << std::hex << err << std::dec << std::endl;
        }
    } else {
        std::cerr << "WARNING: No track data to render (starts=" << m_trackStarts.size()
                  << ", counts=" << m_trackCounts.size() << ")" << std::endl;
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void GLFiberRenderer::getBoundingBox(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const
{
    minX = m_minX;
    maxX = m_maxX;
    minY = m_minY;
    maxY = m_maxY;
    minZ = m_minZ;
    maxZ = m_maxZ;
}

} // namespace DTIFiberLib
