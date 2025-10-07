#include <glad/glad.h>  // MUST be first, before any OpenGL headers
#include "GLFiberWidget.h"
#include "DTIFiberLib.h"
#include <iostream>

GLFiberWidget::GLFiberWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_fiberRenderer(nullptr)
    , m_cameraDistance(200.0f)
    , m_rotationX(0.0f)
    , m_rotationY(0.0f)
    , m_centerX(0.0f)
    , m_centerY(0.0f)
    , m_centerZ(0.0f)
{
    setFocusPolicy(Qt::StrongFocus);
}

GLFiberWidget::~GLFiberWidget()
{
}

void GLFiberWidget::setFiberRenderer(DTIFiberLib::GLFiberRenderer* renderer)
{
    m_fiberRenderer = renderer;
}

void GLFiberWidget::setBoundingBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
{
    // Calculate center
    m_centerX = (minX + maxX) * 0.5f;
    m_centerY = (minY + maxY) * 0.5f;
    m_centerZ = (minZ + maxZ) * 0.5f;

    // Calculate bounding box size
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    float maxSize = std::max({sizeX, sizeY, sizeZ});

    // Adjust camera distance based on data size
    m_cameraDistance = maxSize * 1.5f;

    std::cout << "Data center: (" << m_centerX << ", " << m_centerY << ", " << m_centerZ << ")" << std::endl;
    std::cout << "Camera distance: " << m_cameraDistance << std::endl;

    updateMVPMatrix();
    update();
}

void GLFiberWidget::initializeGL()
{
    // Initialize GLAD (load OpenGL functions)
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);

    // Initialize fiber renderer
    if (m_fiberRenderer) {
        m_fiberRenderer->initialize();
    }
}

void GLFiberWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    // Update projection matrix
    m_projectionMatrix.setToIdentity();
    float aspect = float(w) / float(h > 0 ? h : 1);
    m_projectionMatrix.perspective(45.0f, aspect, 0.1f, 1000.0f);

    updateMVPMatrix();
}

void GLFiberWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_fiberRenderer && m_fiberRenderer->isInitialized()) {
        updateMVPMatrix();
        m_fiberRenderer->render(m_mvpMatrix.constData());
    }
}

void GLFiberWidget::updateMVPMatrix()
{
    // View matrix (camera)
    m_viewMatrix.setToIdentity();
    m_viewMatrix.translate(0.0f, 0.0f, -m_cameraDistance);

    // Model matrix (rotation + centering)
    m_modelMatrix.setToIdentity();
    m_modelMatrix.rotate(m_rotationX, 1.0f, 0.0f, 0.0f);
    m_modelMatrix.rotate(m_rotationY, 0.0f, 1.0f, 0.0f);
    // Center the data at origin
    m_modelMatrix.translate(-m_centerX, -m_centerY, -m_centerZ);

    // MVP = Projection * View * Model
    m_mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
}

void GLFiberWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void GLFiberWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        // Rotate camera
        int dx = event->x() - m_lastMousePos.x();
        int dy = event->y() - m_lastMousePos.y();

        m_rotationY += dx * 0.5f;
        m_rotationX += dy * 0.5f;

        // Clamp rotation
        if (m_rotationX > 89.0f) m_rotationX = 89.0f;
        if (m_rotationX < -89.0f) m_rotationX = -89.0f;

        m_lastMousePos = event->pos();
        update();
    }
}

void GLFiberWidget::wheelEvent(QWheelEvent* event)
{
    // Zoom in/out
    float delta = event->angleDelta().y() / 120.0f;
    m_cameraDistance -= delta * 10.0f;

    // Clamp camera distance
    if (m_cameraDistance < 10.0f) m_cameraDistance = 10.0f;
    if (m_cameraDistance > 500.0f) m_cameraDistance = 500.0f;

    update();
}
