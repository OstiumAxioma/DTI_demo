#include "../header/GLFiberRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace DTIFiberLib {
namespace {

constexpr size_t MAX_CHUNK_BYTES = 32ull * 1024ull * 1024ull; // 32 MB
constexpr int SHADING_GRID = 64;
constexpr float SHADE_DISTANCE_CAP = 4.0f;
constexpr std::array<std::array<int, 2>, 3> SLICE_PLANE_AXES = {
    std::array<int, 2>{1, 2},
    std::array<int, 2>{0, 2},
    std::array<int, 2>{0, 1}
};
constexpr std::array<SliceAxis, 3> AXES = {
    SliceAxis::Sagittal,
    SliceAxis::Coronal,
    SliceAxis::Axial
};

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

class OcclusionVolume {
public:
    void build(const std::vector<GLFiberData::TrackEntry>& tracks,
               const GLFiberData::BoundingBox& box)
    {
        m_valid = false;
        m_boxMinX = box.minX;
        m_boxMaxX = box.maxX;
        m_boxMinY = box.minY;
        m_boxMaxY = box.maxY;
        m_boxMinZ = box.minZ;
        m_boxMaxZ = box.maxZ;

        const float rangeX = std::max(m_boxMaxX - m_boxMinX, 1e-4f);
        const float rangeY = std::max(m_boxMaxY - m_boxMinY, 1e-4f);
        const float rangeZ = std::max(m_boxMaxZ - m_boxMinZ, 1e-4f);

        m_scaleX = (SHADING_GRID - 1) / rangeX;
        m_scaleY = (SHADING_GRID - 1) / rangeY;
        m_scaleZ = (SHADING_GRID - 1) / rangeZ;

        const size_t planeSize = static_cast<size_t>(SHADING_GRID) * static_cast<size_t>(SHADING_GRID);
        const float lowest = std::numeric_limits<float>::lowest();
        const float highest = std::numeric_limits<float>::max();
        m_gridMaxX.assign(planeSize, lowest);
        m_gridMinX.assign(planeSize, highest);
        m_gridMaxY.assign(planeSize, lowest);
        m_gridMinY.assign(planeSize, highest);
        m_gridMaxZ.assign(planeSize, lowest);
        m_gridMinZ.assign(planeSize, highest);

        bool anyPoint = false;

        for (const auto& entry : tracks) {
            for (const auto& point : entry.track) {
                anyPoint = true;
                const int xIdx = clampIndex((point.x - m_boxMinX) * m_scaleX);
                const int yIdx = clampIndex((point.y - m_boxMinY) * m_scaleY);
                const int zIdx = clampIndex((point.z - m_boxMinZ) * m_scaleZ);

                const size_t idxYZ = static_cast<size_t>(yIdx) + static_cast<size_t>(zIdx) * SHADING_GRID;
                const size_t idxXZ = static_cast<size_t>(xIdx) + static_cast<size_t>(zIdx) * SHADING_GRID;
                const size_t idxXY = static_cast<size_t>(xIdx) + static_cast<size_t>(yIdx) * SHADING_GRID;

                m_gridMaxX[idxYZ] = std::max(m_gridMaxX[idxYZ], point.x);
                m_gridMinX[idxYZ] = std::min(m_gridMinX[idxYZ], point.x);
                m_gridMaxY[idxXZ] = std::max(m_gridMaxY[idxXZ], point.y);
                m_gridMinY[idxXZ] = std::min(m_gridMinY[idxXZ], point.y);
                m_gridMaxZ[idxXY] = std::max(m_gridMaxZ[idxXY], point.z);
                m_gridMinZ[idxXY] = std::min(m_gridMinZ[idxXY], point.z);
            }
        }

        if (!anyPoint) {
            return;
        }

        for (size_t i = 0; i < planeSize; ++i) {
            if (m_gridMaxX[i] == lowest) m_gridMaxX[i] = m_boxMaxX;
            if (m_gridMinX[i] == highest) m_gridMinX[i] = m_boxMinX;
            if (m_gridMaxY[i] == lowest) m_gridMaxY[i] = m_boxMaxY;
            if (m_gridMinY[i] == highest) m_gridMinY[i] = m_boxMinY;
            if (m_gridMaxZ[i] == lowest) m_gridMaxZ[i] = m_boxMaxZ;
            if (m_gridMinZ[i] == highest) m_gridMinZ[i] = m_boxMinZ;
        }

        smoothGrid(m_gridMaxX);
        smoothGrid(m_gridMinX);
        smoothGrid(m_gridMaxY);
        smoothGrid(m_gridMinY);
        smoothGrid(m_gridMaxZ);
        smoothGrid(m_gridMinZ);

        m_valid = true;
    }

    bool valid() const { return m_valid; }

    void setStrength(float strength)
    {
        m_strength = std::clamp(strength, 0.0f, 0.1f);
    }

    float occlusion(float x, float y, float z) const
    {
        if (!m_valid) {
            return 1.0f;
        }

        const int xIdx = clampIndex((x - m_boxMinX) * m_scaleX);
        const int yIdx = clampIndex((y - m_boxMinY) * m_scaleY);
        const int zIdx = clampIndex((z - m_boxMinZ) * m_scaleZ);

        const size_t idxYZ = static_cast<size_t>(yIdx) + static_cast<size_t>(zIdx) * SHADING_GRID;
        const size_t idxXZ = static_cast<size_t>(xIdx) + static_cast<size_t>(zIdx) * SHADING_GRID;
        const size_t idxXY = static_cast<size_t>(xIdx) + static_cast<size_t>(yIdx) * SHADING_GRID;

        const float distX = axisDistance(x, m_gridMinX[idxYZ], m_gridMaxX[idxYZ]);
        const float distY = axisDistance(y, m_gridMinY[idxXZ], m_gridMaxY[idxXZ]);
        const float distZ = axisDistance(z, m_gridMinZ[idxXY], m_gridMaxZ[idxXY]);

        const float totalDistance = distX + distY + distZ;
        const float attenuation = std::min(totalDistance * m_strength, 0.95f);
        return std::clamp(1.0f - attenuation + 0.05f, 0.05f, 1.0f);
    }

private:
    static int clampIndex(float value)
    {
        int idx = static_cast<int>(std::round(value));
        if (idx < 0) idx = 0;
        if (idx >= SHADING_GRID) idx = SHADING_GRID - 1;
        return idx;
    }

    static float axisDistance(float pos, float minVal, float maxVal)
    {
        const float positive = std::max(0.0f, maxVal - pos);
        const float negative = std::max(0.0f, pos - minVal);
        return std::min(std::min(positive, negative), SHADE_DISTANCE_CAP);
    }

    void smoothGrid(std::vector<float>& grid)
    {
        std::vector<float> temp(grid.size());
        for (int iteration = 0; iteration < 3; ++iteration) {
            for (int y = 0; y < SHADING_GRID; ++y) {
                for (int x = 0; x < SHADING_GRID; ++x) {
                    float sum = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        const int ny = y + dy;
                        if (ny < 0 || ny >= SHADING_GRID) {
                            continue;
                        }
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int nx = x + dx;
                            if (nx < 0 || nx >= SHADING_GRID) {
                                continue;
                            }
                            sum += grid[ny * SHADING_GRID + nx];
                            ++count;
                        }
                    }
                    temp[y * SHADING_GRID + x] = count > 0 ? sum / static_cast<float>(count) : grid[y * SHADING_GRID + x];
                }
            }
            grid.swap(temp);
        }
    }

    bool m_valid = false;
    float m_boxMinX = 0.0f;
    float m_boxMaxX = 0.0f;
    float m_boxMinY = 0.0f;
    float m_boxMaxY = 0.0f;
    float m_boxMinZ = 0.0f;
    float m_boxMaxZ = 0.0f;
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
    float m_scaleZ = 1.0f;
    std::vector<float> m_gridMaxX;
    std::vector<float> m_gridMinX;
    std::vector<float> m_gridMaxY;
    std::vector<float> m_gridMinY;
    std::vector<float> m_gridMaxZ;
    std::vector<float> m_gridMinZ;
    float m_strength = 0.02f;
};

} // namespace

static const char* vertexShaderSource = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aDirection;
layout(location = 2) in float aShade;

out vec3 vDirection;
out float vShade;
out vec3 vPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
    vDirection = aDirection;
    vShade = aShade;
    vPosition = aPosition;
}
)";

static const char* fragmentShaderSource = R"(
#version 460 core
in vec3 vDirection;
in float vShade;
in vec3 vPosition;
out vec4 FragmentColor;

uniform int uColorMode;
uniform float uOpacity;
uniform int uEnableShading;
uniform int uEnableLighting;
uniform int uLightCount;
uniform vec3 uLightPositions[2];
uniform float uLightAmbient;
uniform float uLightIntensity;

void main() {
    vec3 baseColor = (uColorMode == 1)
        ? normalize(abs(vDirection) + vec3(1e-5))
        : vec3(1.0, 0.0, 0.0);

    if (uEnableShading == 1) {
        baseColor *= vShade;
    }

    float lighting = uLightAmbient;
    if (uEnableLighting == 1) {
        for (int i = 0; i < uLightCount; ++i) {
            vec3 L = uLightPositions[i] - vPosition;
            float dist = length(L);
            if (dist > 1e-4) {
                float atten = uLightIntensity / (1.0 + dist * 0.02 + dist * dist * 0.0002);
                lighting += atten;
            } else {
                lighting += uLightIntensity;
            }
        }
    }

    lighting = clamp(lighting, 0.1, 2.5);
    baseColor *= lighting;

    FragmentColor = vec4(baseColor, uOpacity);
}
)";

static const char* sliceVertexShaderSource = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
    vUV = aUV;
}
)";

static const char* sliceFragmentShaderSource = R"(
#version 460 core
in vec2 vUV;
out vec4 FragmentColor;

uniform sampler2D uSliceTexture;
uniform float uSliceOpacity;

void main() {
    float intensity = texture(uSliceTexture, vUV).r;
    vec3 color = vec3(intensity);
    FragmentColor = vec4(color, uSliceOpacity);
}
)";

GLFiberRenderer::GLFiberRenderer()
    : m_shader(nullptr)
    , m_sliceShader(nullptr)
    , m_chunks()
    , m_data(nullptr)
    , m_dirty(true)
    , m_colorMode(FiberColoringMode::DIRECTION_RGB)
    , m_lineWidth(1.0f)
    , m_opacity(1.0f)
    , m_enableShading(false)
    , m_enableLighting(true)
    , m_lightAmbient(0.35f)
    , m_lightIntensity(0.6f)
    , m_lightingStrengthScale(1.0f)
    , m_renderedTrackCount(0)
    , m_totalPointCount(0)
    , m_minX(0.0f)
    , m_maxX(0.0f)
    , m_minY(0.0f)
    , m_maxY(0.0f)
    , m_minZ(0.0f)
    , m_maxZ(0.0f)
    , m_shadowStrength(0.02f)
    , m_lodEnabled(false)
    , m_maxPointsPerTrack(0)
    , m_initialized(false)
    , m_volume(nullptr)
    , m_sliceOpacity(0.55f)
    , m_hasFiberBounds(false)
    , m_hasVolumeBounds(false)
{
    std::fill(&m_lightPositions[0][0], &m_lightPositions[0][0] + 6, 0.0f);
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

    m_sliceShader = std::make_unique<GLShaderProgram>();
    if (!m_sliceShader->loadFromString(sliceVertexShaderSource, sliceFragmentShaderSource)) {
        std::cerr << "Failed to create slice shader program" << std::endl;
        m_sliceShader.reset();
    }

    m_initialized = true;
    if (m_dirty) {
        rebuildBuffers();
    }
}

void GLFiberRenderer::cleanup()
{
    releaseChunks();
    releaseSlicePlanes();
    m_shader.reset();
    m_sliceShader.reset();
    m_initialized = false;
}

void GLFiberRenderer::setData(const GLFiberData& data)
{
    m_data = &data;
    if (data.empty()) {
        m_hasFiberBounds = false;
        recalcSceneBounds();
    } else {
        updateBoundingBox(data.computeBoundingBox());
    }
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

void GLFiberRenderer::setLightingEnabled(bool enable)
{
    m_enableLighting = enable;
    if (m_enableLighting) {
        updateLighting();
    } else {
        m_lightAmbient = 0.0f;
        m_lightIntensity = 0.0f;
    }
}

void GLFiberRenderer::setLightingStrength(float strengthFactor)
{
    m_lightingStrengthScale = std::clamp(strengthFactor, 0.0f, 5.0f);
    updateLighting();
}

void GLFiberRenderer::setShadowStrength(float strength)
{
    m_shadowStrength = std::clamp(strength, 0.0f, 0.1f);
    m_dirty = true;
}

void GLFiberRenderer::setNiftiVolume(const std::shared_ptr<NiftiVolume>& volume)
{
    if (m_initialized) {
        releaseSlicePlanes();
    } else {
        for (auto& plane : m_slicePlanes) {
            plane = SlicePlane{};
        }
    }

    m_volume = volume;

    if (!m_volume) {
        for (auto& plane : m_slicePlanes) {
            plane = SlicePlane{};
        }
        m_hasVolumeBounds = false;
        recalcSceneBounds();
        return;
    }

    m_volumeBounds = m_volume->computeBoundingBox();
    m_hasVolumeBounds = true;

    for (size_t axisIdx = 0; axisIdx < AXES.size(); ++axisIdx) {
        auto& plane = m_slicePlanes[axisIdx];
        const SliceAxis axis = AXES[axisIdx];
        const int minIndex = m_volume->getExtentMin(axis);
        const int sliceCount = m_volume->getSliceCount(axis);
        plane.voxelIndex = minIndex + sliceCount / 2;
        plane.pixelDirty = true;
        plane.geometryDirty = true;
        plane.texturePendingUpload = true;
        plane.width = 0;
        plane.height = 0;
        plane.pixels.clear();
    }

    recalcSceneBounds();
}

void GLFiberRenderer::setSliceIndex(SliceAxis axis, int voxelIndex)
{
    if (!m_volume) {
        return;
    }
    const size_t axisIdx = static_cast<size_t>(axis);
    const int minIndex = m_volume->getExtentMin(axis);
    const int maxIndex = m_volume->getExtentMax(axis);
    const int clamped = std::clamp(voxelIndex, minIndex, maxIndex);
    auto& plane = m_slicePlanes[axisIdx];
    if (plane.voxelIndex == clamped) {
        return;
    }
    plane.voxelIndex = clamped;
    plane.pixelDirty = true;
    plane.geometryDirty = true;
    plane.texturePendingUpload = true;
}

int GLFiberRenderer::getSliceIndex(SliceAxis axis) const
{
    const size_t axisIdx = static_cast<size_t>(axis);
    return m_slicePlanes[axisIdx].voxelIndex;
}

void GLFiberRenderer::setSliceOpacity(float alpha)
{
    m_sliceOpacity = std::clamp(alpha, 0.0f, 1.0f);
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

    GLFiberData::BoundingBox box{};
    box.minX = m_minX;
    box.maxX = m_maxX;
    box.minY = m_minY;
    box.maxY = m_maxY;
    box.minZ = m_minZ;
    box.maxZ = m_maxZ;

    OcclusionVolume occlusion;
    occlusion.setStrength(m_shadowStrength);
    occlusion.build(tracks, box);

    std::function<float(float, float, float)> shadeFn;
    if (occlusion.valid()) {
        shadeFn = [occlusion](float x, float y, float z) {
            return occlusion.occlusion(x, y, z);
        };
    }

    buildChunksForStyle(GLFiberData::TractStyle::Line, tracks, shadeFn);
    buildChunksForStyle(GLFiberData::TractStyle::Point, tracks, shadeFn);
    buildChunksForStyle(GLFiberData::TractStyle::Tube, tracks, shadeFn);

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
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, strideBytes, reinterpret_cast<void*>(6 * sizeof(float)));

    glBindVertexArray(0);
}

void GLFiberRenderer::buildChunksForStyle(GLFiberData::TractStyle style,
                                          const std::vector<GLFiberData::TrackEntry>& tracks,
                                          const std::function<float(float, float, float)>& shadeFn)
{
    std::vector<float> vertexBuffer;
    vertexBuffer.reserve(1024 * 7);

    auto chunkFactory = [style]() {
        ChunkBuffer chunk;
        chunk.style = style;
        chunk.primitive = (style == GLFiberData::TractStyle::Point) ? GL_POINTS : GL_LINE_STRIP;
        chunk.stride = 7;
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

            float shadeFactor = shadeFn ? shadeFn(track[i].x, track[i].y, track[i].z) : 1.0f;

            vertexBuffer.push_back(track[i].x);
            vertexBuffer.push_back(track[i].y);
            vertexBuffer.push_back(track[i].z);
            vertexBuffer.push_back(dirX);
            vertexBuffer.push_back(dirY);
            vertexBuffer.push_back(dirZ);
            vertexBuffer.push_back(shadeFactor);
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
    m_fiberBounds = box;
    m_hasFiberBounds = true;
    recalcSceneBounds();
}

void GLFiberRenderer::updateLighting()
{
    if (!m_enableLighting) {
        return;
    }

    m_lightPositions[0][0] = m_minX;
    m_lightPositions[0][1] = m_maxY;
    m_lightPositions[0][2] = m_maxZ;

    m_lightPositions[1][0] = m_maxX;
    m_lightPositions[1][1] = m_minY;
    m_lightPositions[1][2] = m_minZ;

    const float dx = m_maxX - m_minX;
    const float dy = m_maxY - m_minY;
    const float dz = m_maxZ - m_minZ;
    const float diag = std::sqrt(std::max(dx * dx + dy * dy + dz * dz, 1e-4f));

    const float scale = std::clamp(m_lightingStrengthScale, 0.0f, 2.0f);
    m_lightAmbient = std::clamp(0.12f + 0.45f * scale, 0.05f, 0.9f);
    m_lightIntensity = std::clamp((0.8f / (0.2f + diag * 0.02f)) * scale, 0.15f, 2.0f);
}

void GLFiberRenderer::releaseSlicePlanes()
{
    for (auto& plane : m_slicePlanes) {
        if (plane.texture != 0 && m_initialized) {
            glDeleteTextures(1, &plane.texture);
        }
        if (plane.vbo != 0 && m_initialized) {
            glDeleteBuffers(1, &plane.vbo);
        }
        if (plane.vao != 0 && m_initialized) {
            glDeleteVertexArrays(1, &plane.vao);
        }
        plane = SlicePlane{};
    }
}

void GLFiberRenderer::updateSlicePlaneResources(size_t axisIdx)
{
    if (!m_volume) {
        return;
    }
    auto& plane = m_slicePlanes[axisIdx];
    if (plane.voxelIndex < 0) {
        return;
    }

    if (plane.pixelDirty) {
        plane.pixelDirty = false;
        if (m_volume->extractSlice(AXES[axisIdx],
                                   plane.voxelIndex,
                                   plane.pixels,
                                   plane.width,
                                   plane.height)) {
            plane.texturePendingUpload = true;
        }
    }

    if (plane.texturePendingUpload) {
        uploadSliceTexture(axisIdx);
    }

    if (plane.geometryDirty) {
        rebuildSliceGeometry(axisIdx);
    }
}

void GLFiberRenderer::uploadSliceTexture(size_t axisIdx)
{
    if (!m_initialized) {
        return;
    }
    auto& plane = m_slicePlanes[axisIdx];
    if (plane.width <= 0 || plane.height <= 0 || plane.pixels.empty()) {
        plane.texturePendingUpload = false;
        return;
    }

    if (plane.texture == 0) {
        glGenTextures(1, &plane.texture);
    }
    glBindTexture(GL_TEXTURE_2D, plane.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_R8,
                 plane.width,
                 plane.height,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 plane.pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    plane.texturePendingUpload = false;
}

void GLFiberRenderer::rebuildSliceGeometry(size_t axisIdx)
{
    if (!m_initialized || !m_volume) {
        return;
    }

    auto& plane = m_slicePlanes[axisIdx];
    if (plane.voxelIndex < 0) {
        return;
    }

    const int primaryAxis = static_cast<int>(axisIdx);
    const int axisA = SLICE_PLANE_AXES[axisIdx][0];
    const int axisB = SLICE_PLANE_AXES[axisIdx][1];

    const SliceAxis axisAEnum = AXES[axisA];
    const SliceAxis axisBEnum = AXES[axisB];

    const double aEdges[2] = {
        static_cast<double>(m_volume->getExtentMin(axisAEnum)) - 0.5,
        static_cast<double>(m_volume->getExtentMax(axisAEnum)) + 0.5
    };
    const double bEdges[2] = {
        static_cast<double>(m_volume->getExtentMin(axisBEnum)) - 0.5,
        static_cast<double>(m_volume->getExtentMax(axisBEnum)) + 0.5
    };

    std::array<float, 20> buffer{};
    size_t idx = 0;
    for (int b = 0; b < 2; ++b) {
        const double voxelB = bEdges[b];
        for (int a = 0; a < 2; ++a) {
            const double voxelA = aEdges[a];
            std::array<double, 3> coords{};
            coords[primaryAxis] = static_cast<double>(plane.voxelIndex);
            coords[axisA] = voxelA;
            coords[axisB] = voxelB;
            const auto world = m_volume->voxelToWorld(coords[0], coords[1], coords[2]);

            buffer[idx++] = static_cast<float>(world[0]);
            buffer[idx++] = static_cast<float>(world[1]);
            buffer[idx++] = static_cast<float>(world[2]);
            buffer[idx++] = (a == 0) ? 0.0f : 1.0f;
            buffer[idx++] = (b == 0) ? 0.0f : 1.0f;
        }
    }

    if (plane.vao == 0) {
        glGenVertexArrays(1, &plane.vao);
    }
    if (plane.vbo == 0) {
        glGenBuffers(1, &plane.vbo);
    }

    glBindVertexArray(plane.vao);
    glBindBuffer(GL_ARRAY_BUFFER, plane.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(buffer.size() * sizeof(float)),
                 buffer.data(),
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(5 * sizeof(float)), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          static_cast<GLsizei>(5 * sizeof(float)),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    plane.geometryDirty = false;
}

void GLFiberRenderer::renderSlices(const float* mvpMatrix)
{
    if (!m_sliceShader || !m_sliceShader->isValid() || !m_volume || m_sliceOpacity <= 0.0f) {
        return;
    }

    bool hasRenderable = false;
    for (size_t axisIdx = 0; axisIdx < AXES.size(); ++axisIdx) {
        updateSlicePlaneResources(axisIdx);
        const auto& plane = m_slicePlanes[axisIdx];
        if (plane.texture != 0 && plane.vao != 0 && plane.width > 0 && plane.height > 0) {
            hasRenderable = true;
        }
    }

    if (!hasRenderable) {
        return;
    }

    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    m_sliceShader->use();
    m_sliceShader->setUniformMatrix4fv("uMVPMatrix", mvpMatrix);
    m_sliceShader->setUniform1i("uSliceTexture", 0);
    m_sliceShader->setUniform1f("uSliceOpacity", m_sliceOpacity);
    glActiveTexture(GL_TEXTURE0);

    for (size_t axisIdx = 0; axisIdx < AXES.size(); ++axisIdx) {
        const auto& plane = m_slicePlanes[axisIdx];
        if (plane.texture == 0 || plane.vao == 0 || plane.width <= 0 || plane.height <= 0) {
            continue;
        }

        glBindVertexArray(plane.vao);
        glBindTexture(GL_TEXTURE_2D, plane.texture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(previousDepthMask);
    glDisable(GL_BLEND);
}

void GLFiberRenderer::recalcSceneBounds()
{
    if (!m_hasFiberBounds && !m_hasVolumeBounds) {
        m_minX = m_minY = m_minZ = -50.0f;
        m_maxX = m_maxY = m_maxZ = 50.0f;
        updateLighting();
        return;
    }

    auto applyBox = [&](const GLFiberData::BoundingBox& box) {
        m_minX = std::min(m_minX, box.minX);
        m_maxX = std::max(m_maxX, box.maxX);
        m_minY = std::min(m_minY, box.minY);
        m_maxY = std::max(m_maxY, box.maxY);
        m_minZ = std::min(m_minZ, box.minZ);
        m_maxZ = std::max(m_maxZ, box.maxZ);
    };

    m_minX = std::numeric_limits<float>::max();
    m_minY = std::numeric_limits<float>::max();
    m_minZ = std::numeric_limits<float>::max();
    m_maxX = std::numeric_limits<float>::lowest();
    m_maxY = std::numeric_limits<float>::lowest();
    m_maxZ = std::numeric_limits<float>::lowest();

    if (m_hasFiberBounds) {
        applyBox(m_fiberBounds);
    }
    if (m_hasVolumeBounds) {
        applyBox(m_volumeBounds);
    }

    updateLighting();
}

void GLFiberRenderer::render(const float* mvpMatrix)
{
    if (!m_initialized || !m_shader || !m_shader->isValid()) {
        return;
    }

    if (m_dirty) {
        rebuildBuffers();
    }

    renderSlices(mvpMatrix);

    if (m_chunks.empty()) {
        return;
    }

    m_shader->use();
    m_shader->setUniformMatrix4fv("uMVPMatrix", mvpMatrix);
    m_shader->setUniform1i("uColorMode", m_colorMode == FiberColoringMode::DIRECTION_RGB ? 1 : 0);
    m_shader->setUniform1f("uOpacity", m_opacity);
    m_shader->setUniform1i("uEnableShading", m_enableShading ? 1 : 0);
    m_shader->setUniform1i("uEnableLighting", m_enableLighting ? 1 : 0);
    m_shader->setUniform1i("uLightCount", m_enableLighting ? 2 : 0);
    m_shader->setUniform1f("uLightAmbient", m_lightAmbient);
    m_shader->setUniform1f("uLightIntensity", m_lightIntensity);
    m_shader->setUniform3fv("uLightPositions", 2, &m_lightPositions[0][0]);

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
