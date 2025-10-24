#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <vector>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;
class QSlider;
class QLabel;
QT_END_NAMESPACE

class GLFiberWidget;

// Forward declarations
namespace DTIFiberLib {
    class TrkFileReader;
    class GLFiberRenderer;
    class GLFiberData;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void setupOpenGLWidget();
    void openTrkFile();
    void toggleShading(bool checked);
    void onLightingSliderChanged(int value);
    void onShadowSliderChanged(int value);

private:
    // UI components
    GLFiberWidget *glWidget;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;
    QAction *openTrkAct;
    QAction *toggleShadingAct;
    QSlider *lightingSlider;
    QSlider *shadowSlider;

    // DTI library components
    std::vector<std::unique_ptr<DTIFiberLib::TrkFileReader>> trkReaders;
    std::unique_ptr<DTIFiberLib::GLFiberData> glFiberData;
    std::unique_ptr<DTIFiberLib::GLFiberRenderer> glFiberRenderer;
    QStringList loadedDatasetNames;

    size_t appendTracksToRenderer(const QString& filePath,
                                  DTIFiberLib::TrkFileReader& reader);
};

#endif // MAINWINDOW_H
