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
    m_totalPointCount = 0;
    m_renderedTrackCount = 0;

    for (const auto& track : m_tracks) {
        if (track.empty()) continue;

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

    // Calculate bounding box for debugging
    float minX = 1e10, minY = 1e10, minZ = 1e10;
    float maxX = -1e10, maxY = -1e10, maxZ = -1e10;
    for (size_t i = 0; i < m_vertexData.size(); i += 6) {
        float x = m_vertexData[i];
        float y = m_vertexData[i + 1];
        float z = m_vertexData[i + 2];
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
    }

    std::cout << "Built vertex data: " << m_renderedTrackCount << " tracks, "
              << m_totalPointCount << " points" << std::endl;
    std::cout << "Bounding box: X[" << minX << ", " << maxX << "] "
              << "Y[" << minY << ", " << maxY << "] "
              << "Z[" << minZ << ", " << maxZ << "]" << std::endl;
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

    // Build vertex data
    buildVertexData();

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

    // Render each track as a line strip
    size_t vertexOffset = 0;
    for (const auto& track : m_tracks) {
        if (track.empty()) continue;

        glDrawArrays(GL_LINE_STRIP, static_cast<GLint>(vertexOffset), static_cast<GLsizei>(track.size()));
        vertexOffset += track.size();
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

} // namespace DTIFiberLib
