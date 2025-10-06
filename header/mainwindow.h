#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;
QT_END_NAMESPACE

class GLFiberWidget;

// Forward declarations
namespace DTIFiberLib {
    class TrkFileReader;
    class GLFiberRenderer;
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

private:
    // UI components
    GLFiberWidget *glWidget;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;
    QAction *openTrkAct;

    // DTI library components
    std::unique_ptr<DTIFiberLib::TrkFileReader> trkReader;
    std::unique_ptr<DTIFiberLib::GLFiberRenderer> glFiberRenderer;
};

#endif // MAINWINDOW_H