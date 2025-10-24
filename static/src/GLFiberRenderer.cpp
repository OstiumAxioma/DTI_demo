#include "../header/GLFiberRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace DTIFiberLib {

namespace {
constexpr size_t MAX_CHUNK_BYTES = 32ull * 1024ull * 1024ull; // 32 MB
constexpr float LIGHT_DIR[3] = {0.3f, 0.6f, 0.7f};

inline void computeDirection(const FiberTrack& track, size_t index, float& dirX, float& dirY, float& dirZ)
{
    dirX = dirY = dirZ = 0.0f;
    if (track.size() == 1) {
        dirX = dirY = dirZ = 0.5f;
        return;
    }

    if (index == 0) {
        dirX = track[1].x - track[0].x;
        dirY = track[1].y - track[0].y;
        dirZ = track[1].z - track[0].z;
    } else if (index == track.size() - 1) {
        dirX = track[index].x - track[index - 1].x;
        dirY = track[index].y - track[index - 1].y;
        dirZ = track[index].z - track[index - 1].z;
    } else {
        dirX = track[index + 1].x - track[index - 1].x;
        dirY = track[index + 1].y - track[index - 1].y;
        dirZ = track[index + 1].z - track[index - 1].z;
    }

    const float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length > 0.0001f) {
        dirX /= length;
        dirY /= length;
        dirZ /= length;
    }
}

} // namespace

// Embedded shaders
static const char* vertexShaderSource = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aDirection;

out vec3 vDirection;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
    vDirection = aDirection;
}
)";

static const char* fragmentShaderSource = R"(
#version 460 core
in vec3 vDirection;
out vec4 FragmentColor;

uniform int uColorMode;
uniform float uOpacity;
uniform int uEnableShading;
uniform vec3 uLightDir;

void main() {
    vec3 baseColor = (uColorMode == 1)
        ? normalize(abs(vDirection) + vec3(1e-5))
        : vec3(1.0, 0.0, 0.0);

    if (uEnableShading == 1) {
        vec3 L = normalize(uLightDir);
        if (length(L) < 1e-5) {
            L = vec3(0.0, 0.0, 1.0);
        }

        float lenDir = length(vDirection);
        vec3 dirNorm = (lenDir > 1e-5) ? vDirection / lenDir : vec3(0.0, 0.0, 1.0);

        float alignment = abs(dot(dirNorm, L));
        float brightness = mix(0.25, 1.0, alignment);
        baseColor = clamp(baseColor * brightness, 0.0, 1.0);
    }

    FragmentColor = vec4(baseColor, uOpacity);
}
)";

GLFiberRenderer::GLFiberRenderer()
    : m_shader(nullptr)
    , m_chunks()
    , m_data(nullptr)
    , m_dirty(true)
    , m_colorMode(FiberColoringMode::DIRECTION_RGB)
    , m_lineWidth(1.0f)
    , m_opacity(1.0f)
    , m_enableShading(false)
    , m_renderedTrackCount(0)
    , m_totalPointCount(0)
    , m_minX(0.0f)
    , m_maxX(0.0f)
    , m_minY(0.0f)
    , m_maxY(0.0f)
    , m_minZ(0.0f)
    , m_maxZ(0.0f)
    , m_lodEnabled(false)
    , m_maxPointsPerTrack(0)
    , m_initialized(false)
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

    m_shader = std::make_unique<GLShaderProgram>();
    if (!m_shader->loadFromString(vertexShaderSource, fragmentShaderSource)) {
        std::cerr << "Failed to create shader program" << std::endl;
        m_shader.reset();
        return;
    }

    m_initialized = true;
    if (m_dirty) {
        rebuildBuffers();
    }
}

void GLFiberRenderer::cleanup()
{
    releaseChunks();
    m_shader.reset();
    m_initialized = false;
}

void GLFiberRenderer::setData(const GLFiberData& data)
{
    m_data = &data;
    updateBoundingBox(data.computeBoundingBox());
    m_dirty = true;
}

void GLFiberRenderer::setColorMode(FiberColoringMode mode)
{
    m_colorMode = mode;
}

void GLFiberRenderer::setLineWidth(float width)
{
    m_lineWidth = std::max(1.0f, width);
}

void GLFiberRenderer::setOpacity(float opacity)
{
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void GLFiberRenderer::setShadingEnabled(bool enable)
{
    m_enableShading = enable;
}

void GLFiberRenderer::setLODEnabled(bool enable)
{
    m_lodEnabled = enable;
    m_dirty = true;
}

void GLFiberRenderer::setMaxPointsPerTrack(size_t maxPoints)
{
    m_maxPointsPerTrack = maxPoints;
    m_dirty = true;
}

void GLFiberRenderer::rebuildBuffers()
{
    if (!m_initialized) {
        m_dirty = true;
        return;
    }

    releaseChunks();
    m_renderedTrackCount = 0;
    m_totalPointCount = 0;

    if (!m_data || m_data->empty()) {
        m_dirty = false;
        return;
    }

    const auto& tracks = m_data->getTracks();

    buildChunksForStyle(GLFiberData::TractStyle::Line, tracks);
    buildChunksForStyle(GLFiberData::TractStyle::Point, tracks);
    buildChunksForStyle(GLFiberData::TractStyle::Tube, tracks);

    m_dirty = false;
}

void GLFiberRenderer::releaseChunks()
{
    for (auto& chunk : m_chunks) {
        if (chunk.vbo != 0) {
            glDeleteBuffers(1, &chunk.vbo);
        }
        if (chunk.vao != 0) {
            glDeleteVertexArrays(1, &chunk.vao);
        }
    }
    m_chunks.clear();
}

void GLFiberRenderer::setupChunkAttributes(ChunkBuffer& chunk) const
{
    glBindVertexArray(chunk.vao);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);

    const GLsizei strideBytes = chunk.stride * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

void GLFiberRenderer::buildChunksForStyle(GLFiberData::TractStyle style,
                                          const std::vector<GLFiberData::TrackEntry>& tracks)
{
    std::vector<float> vertexBuffer;
    vertexBuffer.reserve(1024 * 6);

    auto chunkFactory = [style]() {
        ChunkBuffer chunk;
        chunk.style = style;
        chunk.primitive = (style == GLFiberData::TractStyle::Point) ? GL_POINTS : GL_LINE_STRIP;
        chunk.stride = 6;
        return chunk;
    };

    ChunkBuffer chunk = chunkFactory();

    const size_t maxPointsLimit = (m_lodEnabled && m_maxPointsPerTrack > 0)
                                      ? m_maxPointsPerTrack
                                      : std::numeric_limits<size_t>::max();

    for (const auto& entry : tracks) {
        if (entry.style != style) {
            continue;
        }

        const auto& track = entry.track;
        if (track.empty()) {
            continue;
        }

        const size_t pointBudget = std::min(track.size(), maxPointsLimit);
        const size_t startIndex = vertexBuffer.size() / chunk.stride;
        size_t emittedPoints = 0;

        for (size_t i = 0; i < pointBudget; ++i) {
            float dirX = 0.0f;
            float dirY = 0.0f;
            float dirZ = 0.0f;
            computeDirection(track, i, dirX, dirY, dirZ);

            vertexBuffer.push_back(track[i].x);
            vertexBuffer.push_back(track[i].y);
            vertexBuffer.push_back(track[i].z);
            vertexBuffer.push_back(dirX);
            vertexBuffer.push_back(dirY);
            vertexBuffer.push_back(dirZ);
            ++emittedPoints;
        }

        if (emittedPoints == 0) {
            continue;
        }

        chunk.starts.push_back(static_cast<GLint>(startIndex));
        chunk.counts.push_back(static_cast<GLsizei>(emittedPoints));
        chunk.vertexCount += emittedPoints;

        m_renderedTrackCount++;
        m_totalPointCount += emittedPoints;

        const size_t currentBytes = vertexBuffer.size() * sizeof(float);
        if (currentBytes >= MAX_CHUNK_BYTES) {
            if (!chunk.starts.empty()) {
                glGenVertexArrays(1, &chunk.vao);
                glGenBuffers(1, &chunk.vbo);
                glBindVertexArray(chunk.vao);
                glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(vertexBuffer.size() * sizeof(float)),
                             vertexBuffer.data(),
                             GL_STATIC_DRAW);
                setupChunkAttributes(chunk);
                glBindVertexArray(0);
                m_chunks.push_back(chunk);
            }

            vertexBuffer.clear();
            chunk = chunkFactory();
        }
    }

    if (!vertexBuffer.empty() && !chunk.starts.empty()) {
        glGenVertexArrays(1, &chunk.vao);
        glGenBuffers(1, &chunk.vbo);
        glBindVertexArray(chunk.vao);
        glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexBuffer.size() * sizeof(float)),
                     vertexBuffer.data(),
                     GL_STATIC_DRAW);
        setupChunkAttributes(chunk);
        glBindVertexArray(0);
        m_chunks.push_back(chunk);
    }
}

void GLFiberRenderer::updateBoundingBox(const GLFiberData::BoundingBox& box)
{
    m_minX = box.minX;
    m_maxX = box.maxX;
    m_minY = box.minY;
    m_maxY = box.maxY;
    m_minZ = box.minZ;
    m_maxZ = box.maxZ;
}

void GLFiberRenderer::render(const float* mvpMatrix)
{
    if (!m_initialized || !m_shader || !m_shader->isValid()) {
        return;
    }

    if (m_dirty) {
        rebuildBuffers();
    }

    if (m_chunks.empty()) {
        return;
    }

    m_shader->use();
    m_shader->setUniformMatrix4fv("uMVPMatrix", mvpMatrix);
    m_shader->setUniform1i("uColorMode", m_colorMode == FiberColoringMode::DIRECTION_RGB ? 1 : 0);
    m_shader->setUniform1f("uOpacity", m_opacity);
    m_shader->setUniform1i("uEnableShading", m_enableShading ? 1 : 0);
    m_shader->setUniform3f("uLightDir", LIGHT_DIR[0], LIGHT_DIR[1], LIGHT_DIR[2]);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& chunk : m_chunks) {
        if (chunk.starts.empty()) {
            continue;
        }

        glBindVertexArray(chunk.vao);

        const float effectiveWidth =
            (chunk.style == GLFiberData::TractStyle::Tube) ? std::max(3.0f * m_lineWidth, 2.0f) : m_lineWidth;

        if (chunk.primitive == GL_POINTS) {
            glPointSize(effectiveWidth);
        } else {
            glLineWidth(effectiveWidth);
        }

        glMultiDrawArrays(chunk.primitive,
                          chunk.starts.data(),
                          chunk.counts.data(),
                          static_cast<GLsizei>(chunk.counts.size()));
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
