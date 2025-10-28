#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <array>
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
class QListWidget;
class QListWidgetItem;
class QDockWidget;
QT_END_NAMESPACE

class GLFiberWidget;

// Forward declarations
namespace DTIFiberLib {
    class TrkFileReader;
    class GLFiberRenderer;
    class GLFiberData;
    class NiftiVolume;
    enum class SliceAxis;
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
    void openNiftiFile();
    void toggleShading(bool checked);
    void onLightingSliderChanged(int value);
    void onShadowSliderChanged(int value);
    void onSagittalSliceChanged(int value);
    void onCoronalSliceChanged(int value);
    void onAxialSliceChanged(int value);

private:
    // UI components
    GLFiberWidget *glWidget;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;
    QAction *openTrkAct;
    QAction *openNiftiAct;
    QAction *toggleShadingAct;
    QSlider *lightingSlider;
    QSlider *shadowSlider;
    QSlider *sagittalSlider;
    QSlider *coronalSlider;
    QSlider *axialSlider;
    QDockWidget *datasetDock;
    QListWidget *datasetList;
    bool suppressDatasetSignal;

    // DTI library components
    std::vector<std::unique_ptr<DTIFiberLib::TrkFileReader>> trkReaders;
    std::unique_ptr<DTIFiberLib::GLFiberData> glFiberData;
    std::unique_ptr<DTIFiberLib::GLFiberRenderer> glFiberRenderer;
    QStringList loadedDatasetNames;
    std::shared_ptr<DTIFiberLib::NiftiVolume> niftiVolume;
    std::array<int, 3> niftiExtentMin;
    std::array<double, 3> niftiSpacing;

    size_t appendTracksToRenderer(const QString& filePath,
                                  DTIFiberLib::TrkFileReader& reader);
    void applySliceSlider(DTIFiberLib::SliceAxis axis, int sliderValue);
    QString sliceAxisLabel(DTIFiberLib::SliceAxis axis) const;
    void createDatasetDock();
    void refreshDatasetList();
    void handleDatasetVisibilityChange(QListWidgetItem* item);
};

#endif // MAINWINDOW_H
