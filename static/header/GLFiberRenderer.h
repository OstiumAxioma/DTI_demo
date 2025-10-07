#ifndef GLFIBERRENDERER_H
#define GLFIBERRENDERER_H

#include "TrkFileReader.h"
#include "GLShaderProgram.h"
#include <memory>
#include <vector>
#include <glad/glad.h>

namespace DTIFiberLib {

enum class FiberColoringMode {
    SOLID_COLOR,
    DIRECTION_RGB,
    RANDOM_COLORS
};

/**
 * OpenGL Fiber Bundle Renderer
 * High-performance renderer for DTI fiber tracts using OpenGL
 * Replaces VTK-based rendering to handle millions of tracks
 */
class GLFiberRenderer {
public:
    GLFiberRenderer();
    ~GLFiberRenderer();

    // Data interface
    void setTracks(const std::vector<FiberTrack>& tracks);
    void setColorMode(FiberColoringMode mode);
    void setLineWidth(float width);
    void setOpacity(float opacity);

    // Rendering control
    void initialize();  // Must be called after OpenGL context is created
    void render(const float* mvpMatrix);  // Render with Model-View-Projection matrix
    void cleanup();

    // Performance control
    void setLODEnabled(bool enable);
    void setMaxPointsPerTrack(size_t maxPoints);

    // Statistics
    size_t getRenderedTrackCount() const { return m_renderedTrackCount; }
    size_t getTotalPointCount() const { return m_totalPointCount; }

    // Bounding box
    void getBoundingBox(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const;

    bool isInitialized() const { return m_initialized; }

private:
    void uploadToGPU();
    void buildVertexData();
    void calculateDirectionColors();

    // OpenGL resources
    GLuint m_VAO;
    GLuint m_VBO;
    std::unique_ptr<GLShaderProgram> m_shader;

    // Data
    std::vector<FiberTrack> m_tracks;
    std::vector<float> m_vertexData;  // Interleaved: pos.x, pos.y, pos.z, dir.x, dir.y, dir.z
    std::vector<GLint> m_trackStarts;  // Start index of each track
    std::vector<GLsizei> m_trackCounts;  // Point count of each track

    // Rendering state
    FiberColoringMode m_colorMode;
    float m_lineWidth;
    float m_opacity;

    // Statistics
    size_t m_renderedTrackCount;
    size_t m_totalPointCount;

    // Bounding box
    float m_minX, m_maxX, m_minY, m_maxY, m_minZ, m_maxZ;

    // Performance options
    bool m_lodEnabled;
    size_t m_maxPointsPerTrack;

    bool m_initialized;
    bool m_needsUpload;
};

} // namespace DTIFiberLib

#endif // GLFIBERRENDERER_H
