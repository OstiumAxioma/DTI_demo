#include "mainwindow.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QVTKOpenGLWidget.h>

// 包含静态库头文件
#include "DTIFiberLib.h"
#include "TrkFileReader.h"
#include "FiberBundleRenderer.h"

// VTK头文件
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , vtkWidget(nullptr)
    , dtiRenderer(std::make_unique<DTIFiberLib::DTIFiberRenderer>())
    , trkReader(std::make_unique<DTIFiberLib::TrkFileReader>())
    , fiberRenderer(std::make_unique<DTIFiberLib::FiberBundleRenderer>())
{
    setWindowTitle("DTI Fiber Viewer");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    
    // 先创建一个简单的界面，确保程序能稳定运行
    setupSimpleWidget();
    
    // 延迟初始化VTK组件
    QTimer::singleShot(2000, this, &MainWindow::setupVTKWidget);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
    // 退出动作
    exitAct = new QAction("退出(&Q)", this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip("退出应用程序");
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // 关于动作
    aboutAct = new QAction("关于(&A)", this);
    aboutAct->setStatusTip("显示应用程序的关于对话框");
    connect(aboutAct, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 DTI Fiber Viewer",
                          "这是一个基于VTK和Qt的DTI神经纤维束可视化项目。\n用于加载和显示.trk文件。");
    });

    // 打开TRK文件动作
    openTrkAct = new QAction("打开TRK文件(&T)", this);
    openTrkAct->setShortcut(QKeySequence::Open);
    openTrkAct->setStatusTip("打开TrackVis .trk文件");
    connect(openTrkAct, &QAction::triggered, this, &MainWindow::openTrkFile);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(openTrkAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar("文件");
    fileToolBar->addAction(openTrkAct);
    fileToolBar->addAction(exitAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("就绪");
}

void MainWindow::setupSimpleWidget()
{
    QLabel *label = new QLabel("DTI Fiber Viewer 正在初始化...", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel { font-size: 18px; color: blue; }");
    setCentralWidget(label);
    statusBar()->showMessage("简单界面创建成功", 2000);
}

void MainWindow::setupVTKWidget()
{
    try {
        statusBar()->showMessage("正在初始化VTK...", 1000);

        // 步骤1：创建QVTKOpenGLWidget
        vtkWidget = new QVTKOpenGLWidget(this);
        setCentralWidget(vtkWidget);

        // 步骤2：初始化静态库中的DTI渲染器
        dtiRenderer->InitializeRenderer();
        dtiRenderer->SetBackground(0.1, 0.2, 0.4); // 深蓝色背景

        // 步骤3：将渲染器与QVTKOpenGLWidget连接
        vtkRenderWindow* renderWindow = vtkWidget->GetRenderWindow();
        dtiRenderer->SetRenderWindow(renderWindow);

        statusBar()->showMessage("VTK集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("VTK初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("VTK初始化失败", 3000);
    }
}


void MainWindow::openTrkFile()
{
    try {
        QString fileName = QFileDialog::getOpenFileName(
            this, 
            "打开TRK文件", 
            "data", 
            "TRK Files (*.trk);;All Files (*)"
        );
        
        if (!fileName.isEmpty()) {
            statusBar()->showMessage("正在读取TRK文件...", 1000);
            
            if (trkReader->LoadTractographyFile(fileName.toStdString())) {
                trkReader->PrintHeaderInfo();
                
                size_t trackCount = trkReader->GetTrackCount();
                
                // 渲染纤维束
                statusBar()->showMessage("正在渲染纤维束...", 1000);
                
                const auto& tracks = trkReader->GetAllTracks();
                fiberRenderer->SetTrackSubset(tracks, 1000);  // 限制显示1000条轨迹以提高性能
                fiberRenderer->SetColoringMode(DTIFiberLib::FiberColoringMode::DIRECTION_RGB);
                fiberRenderer->SetLineWidth(1.0f);
                fiberRenderer->RenderFiberBundle();
                
                // 添加到VTK渲染器
                dtiRenderer->ClearActors();  // 清除之前的对象
                dtiRenderer->GetRenderer()->AddActor(fiberRenderer->GetActor());
                dtiRenderer->ResetCamera();
                dtiRenderer->Render();
                
                QString successMsg = QString("成功渲染 %1 条纤维束（共 %2 条）")
                    .arg(fiberRenderer->GetRenderedTrackCount())
                    .arg(trackCount);
                statusBar()->showMessage(successMsg, 5000);
                
                QMessageBox::information(this, "读取和渲染成功", 
                    QString("文件：%1\n总轨迹数量：%2\n已渲染：%3 条\n总点数：%4")
                    .arg(QFileInfo(fileName).fileName())
                    .arg(trackCount)
                    .arg(fiberRenderer->GetRenderedTrackCount())
                    .arg(fiberRenderer->GetTotalPointCount()));
                    
            } else {
                QString errorMsg = QString::fromStdString(trkReader->GetLastErrorMessage());
                QMessageBox::warning(this, "读取失败", 
                    QString("无法读取TRK文件：\n%1\n\n错误信息：%2")
                    .arg(fileName)
                    .arg(errorMsg));
                statusBar()->showMessage("TRK文件读取失败", 3000);
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", 
            QString("读取TRK文件时发生异常：%1").arg(e.what()));
        statusBar()->showMessage("读取TRK文件异常", 3000);
    }
}