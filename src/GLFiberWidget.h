#ifndef GLFIBERWIDGET_H
#define GLFIBERWIDGET_H

#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <memory>

// Forward declaration
namespace DTIFiberLib {
    class GLFiberRenderer;
}

/**
 * OpenGL Widget for rendering fiber bundles
 * Handles OpenGL context, camera control, and user interaction
 * Note: Uses GLAD for OpenGL function loading (not Qt's OpenGL functions)
 */
class GLFiberWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit GLFiberWidget(QWidget* parent = nullptr);
    ~GLFiberWidget();

    void setFiberRenderer(DTIFiberLib::GLFiberRenderer* renderer);

protected:
    // Qt OpenGL interface
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // Mouse interaction
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateMVPMatrix();

    DTIFiberLib::GLFiberRenderer* m_fiberRenderer;

    // Camera parameters
    float m_cameraDistance;
    float m_rotationX;
    float m_rotationY;
    QPoint m_lastMousePos;

    // Matrices
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_modelMatrix;
    QMatrix4x4 m_mvpMatrix;
};

#endif // GLFIBERWIDGET_H
