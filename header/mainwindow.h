#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// VTK模块初始化 - 解决 "no override found for 'vtkRenderer'" 错误
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2)
VTK_MODULE_INIT(vtkInteractionStyle)

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;
QT_END_NAMESPACE

class QVTKOpenGLWidget;

// 前向声明
namespace DTIFiberLib {
    class DTIFiberRenderer;
    class TrkFileReader;
    class FiberBundleRenderer;
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
    void setupSimpleWidget();
    void setupVTKWidget();
    void openTrkFile();

private:
    // UI组件
    QVTKOpenGLWidget *vtkWidget;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;
    QAction *openTrkAct;

    // 使用静态库的DTI纤维渲染器和TRK文件读取器
    std::unique_ptr<DTIFiberLib::DTIFiberRenderer> dtiRenderer;
    std::unique_ptr<DTIFiberLib::TrkFileReader> trkReader;
    std::unique_ptr<DTIFiberLib::FiberBundleRenderer> fiberRenderer;
};

#endif // MAINWINDOW_H