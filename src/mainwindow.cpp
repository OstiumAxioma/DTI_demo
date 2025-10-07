// MUST include GLAD first, before any Qt OpenGL headers
#include <glad/glad.h>

#include "mainwindow.h"
#include "GLFiberWidget.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QDir>
#include <random>
#include <algorithm>
#include <iostream>

// Include static library header - unified entry
#include "DTIFiberLib.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , glWidget(nullptr)
    , trkReader(std::make_unique<DTIFiberLib::TrkFileReader>())
    , glFiberRenderer(std::make_unique<DTIFiberLib::GLFiberRenderer>())
{
    setWindowTitle("DTI Fiber Viewer - OpenGL");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    // Initialize OpenGL widget
    setupOpenGLWidget();
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

    // About action
    aboutAct = new QAction("关于(&A)", this);
    aboutAct->setStatusTip("显示应用程序的关于对话框");
    connect(aboutAct, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 DTI Fiber Viewer",
                          "这是一个基于OpenGL和Qt的DTI神经纤维束可视化项目。\n用于加载和显示.trk文件。");
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

void MainWindow::setupOpenGLWidget()
{
    try {
        statusBar()->showMessage("正在初始化OpenGL...", 1000);

        // Create OpenGL widget
        glWidget = new GLFiberWidget(this);
        setCentralWidget(glWidget);

        // Set the fiber renderer
        glWidget->setFiberRenderer(glFiberRenderer.get());

        statusBar()->showMessage("OpenGL集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("OpenGL初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("OpenGL初始化失败", 3000);
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

                // Render fiber bundles with OpenGL
                statusBar()->showMessage("正在渲染纤维束...", 1000);

                const auto& allTracks = trkReader->GetAllTracks();

                // Auto-downsample if too many tracks (>500K)
                std::vector<DTIFiberLib::FiberTrack> tracksToRender;
                size_t maxTracks = 500000;  // 50万条限制
                if (allTracks.size() > maxTracks) {
                    // Random sampling using reservoir sampling (efficient for large datasets)
                    tracksToRender.reserve(maxTracks);
                    std::random_device rd;
                    std::mt19937 gen(rd());

                    // Reservoir sampling algorithm
                    for (size_t i = 0; i < allTracks.size(); ++i) {
                        if (i < maxTracks) {
                            tracksToRender.push_back(allTracks[i]);
                        } else {
                            std::uniform_int_distribution<size_t> dist(0, i);
                            size_t j = dist(gen);
                            if (j < maxTracks) {
                                tracksToRender[j] = allTracks[i];
                            }
                        }
                    }

                    std::cout << "Downsampled " << allTracks.size() << " tracks to "
                              << tracksToRender.size() << " (reservoir sampling)" << std::endl;
                } else {
                    tracksToRender = allTracks;
                }

                glFiberRenderer->setTracks(tracksToRender);  // This now builds vertex data and calculates bounding box
                glFiberRenderer->setColorMode(DTIFiberLib::FiberColoringMode::DIRECTION_RGB);
                glFiberRenderer->setLineWidth(2.0f);

                // Set bounding box for automatic camera positioning
                float minX, maxX, minY, maxY, minZ, maxZ;
                glFiberRenderer->getBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);
                glWidget->setBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);

                // Update OpenGL widget
                glWidget->update();

                QString successMsg = QString("成功加载 %1 条纤维束")
                    .arg(trackCount);
                statusBar()->showMessage(successMsg, 5000);

                // Export JSON (optional)
                QDir dataDir("data");
                if (!dataDir.exists()) {
                    dataDir.mkpath(".");
                }
                QString jsonPath = "data/" + QFileInfo(fileName).baseName() + "_export.json";
                bool jsonExported = trkReader->ExportToJSON(jsonPath.toStdString(), 10);

                if (jsonExported) {
                    QMessageBox::information(this, "加载成功",
                        QString("文件：%1\n轨迹数量：%2\n总点数：%3\n\nJSON已导出至：%4")
                        .arg(QFileInfo(fileName).fileName())
                        .arg(trackCount)
                        .arg(glFiberRenderer->getTotalPointCount())
                        .arg(QFileInfo(jsonPath).absoluteFilePath()));
                } else {
                    QMessageBox::information(this, "加载成功",
                        QString("文件：%1\n轨迹数量：%2\n总点数：%3\n\n(JSON导出失败)")
                        .arg(QFileInfo(fileName).fileName())
                        .arg(trackCount)
                        .arg(glFiberRenderer->getTotalPointCount()));
                }

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