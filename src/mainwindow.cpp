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
#include <QSlider>
#include <QHBoxLayout>
#include <QDir>
#include <QStringList>
#include <random>
#include <algorithm>
#include <iostream>

// Include static library header - unified entry
#include "DTIFiberLib.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , glWidget(nullptr)
    , glFiberData(std::make_unique<DTIFiberLib::GLFiberData>())
    , glFiberRenderer(std::make_unique<DTIFiberLib::GLFiberRenderer>())
    , lightingSlider(nullptr)
    , shadowSlider(nullptr)
{
    setWindowTitle("DTI Fiber Viewer - OpenGL");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    // Initialize OpenGL widget
    setupOpenGLWidget();

    if (lightingSlider) {
        onLightingSliderChanged(lightingSlider->value());
    }
    if (shadowSlider) {
        onShadowSliderChanged(shadowSlider->value());
    }
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

    toggleShadingAct = new QAction("启用阴影(&S)", this);
    toggleShadingAct->setCheckable(true);
    toggleShadingAct->setStatusTip("切换纤维束阴影效果");
    connect(toggleShadingAct, &QAction::toggled, this, &MainWindow::toggleShading);
}

size_t MainWindow::appendTracksToRenderer(const QString& filePath,
                                          DTIFiberLib::TrkFileReader& reader)
{
    const auto& allTracks = reader.GetAllTracks();
    if (allTracks.empty()) {
        return 0;
    }

    std::vector<DTIFiberLib::FiberTrack> tracksToAppend;
    const size_t maxTracks = 500000;
    if (allTracks.size() > maxTracks) {
        tracksToAppend.reserve(maxTracks);
        std::random_device rd;
        std::mt19937 gen(rd());
        for (size_t i = 0; i < allTracks.size(); ++i) {
            if (i < maxTracks) {
                tracksToAppend.push_back(allTracks[i]);
            } else {
                std::uniform_int_distribution<size_t> dist(0, i);
                size_t j = dist(gen);
                if (j < maxTracks) {
                    tracksToAppend[j] = allTracks[i];
                }
            }
        }
        std::cout << "Downsampled " << allTracks.size() << " tracks from "
                  << QFileInfo(filePath).fileName().toStdString()
                  << " to " << tracksToAppend.size() << " entries.\n";
    } else {
        tracksToAppend = allTracks;
    }

    const QString datasetName = QFileInfo(filePath).completeBaseName();
    glFiberData->addDataset(datasetName.toStdString(),
                            tracksToAppend,
                            DTIFiberLib::GLFiberData::TractStyle::Line);

    // Export a small JSON snapshot for diagnostics
    QDir dataDir("data");
    if (!dataDir.exists()) {
        dataDir.mkpath(".");
    }
    const QString jsonPath = "data/" + datasetName + "_export.json";
    reader.ExportToJSON(jsonPath.toStdString(), 10);

    return tracksToAppend.size();
}

void MainWindow::toggleShading(bool checked)
{
    if (glFiberRenderer) {
        glFiberRenderer->setShadingEnabled(checked);
        glWidget->update();
    }
}

void MainWindow::onLightingSliderChanged(int value)
{
    if (!glFiberRenderer || !glWidget) {
        return;
    }

    float factor = static_cast<float>(value) / 50.0f;
    if (factor < 0.0f) factor = 0.0f;
    glFiberRenderer->setLightingEnabled(factor > 0.01f);
    glFiberRenderer->setLightingStrength(factor);
    glWidget->update();
}

void MainWindow::onShadowSliderChanged(int value)
{
    if (!glFiberRenderer || !glWidget) {
        return;
    }

    float strength = static_cast<float>(value) / 100.0f * 0.08f;
    glFiberRenderer->setShadowStrength(strength);
    glWidget->update();
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(openTrkAct);
    fileMenu->addAction(toggleShadingAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar("文件");
    fileToolBar->addAction(openTrkAct);
    fileToolBar->addAction(toggleShadingAct);
    fileToolBar->addAction(exitAct);

    QWidget* lightingWidget = new QWidget(fileToolBar);
    QHBoxLayout* lightLayout = new QHBoxLayout(lightingWidget);
    lightLayout->setContentsMargins(6, 0, 6, 0);
    QLabel* lightLabel = new QLabel("光照", lightingWidget);
    lightingSlider = new QSlider(Qt::Horizontal, lightingWidget);
    lightingSlider->setRange(0, 100);
    lightingSlider->setValue(70);
    lightingSlider->setFixedWidth(120);
    lightingSlider->setToolTip("调整全局光照强度");
    lightLayout->addWidget(lightLabel);
    lightLayout->addWidget(lightingSlider);
    fileToolBar->addWidget(lightingWidget);
    connect(lightingSlider, &QSlider::valueChanged, this, &MainWindow::onLightingSliderChanged);

    QWidget* shadowWidget = new QWidget(fileToolBar);
    QHBoxLayout* shadowLayout = new QHBoxLayout(shadowWidget);
    shadowLayout->setContentsMargins(6, 0, 6, 0);
    QLabel* shadowLbl = new QLabel("阴影", shadowWidget);
    shadowSlider = new QSlider(Qt::Horizontal, shadowWidget);
    shadowSlider->setRange(0, 100);
    shadowSlider->setValue(35);
    shadowSlider->setFixedWidth(120);
    shadowSlider->setToolTip("调整遮挡阴影强度");
    shadowLayout->addWidget(shadowLbl);
    shadowLayout->addWidget(shadowSlider);
    fileToolBar->addWidget(shadowWidget);
    connect(shadowSlider, &QSlider::valueChanged, this, &MainWindow::onShadowSliderChanged);
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
        glFiberRenderer->setShadingEnabled(toggleShadingAct->isChecked());

        statusBar()->showMessage("OpenGL集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("OpenGL初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("OpenGL初始化失败", 3000);
    }
}


void MainWindow::openTrkFile()
{
    try {
        const QStringList fileNames = QFileDialog::getOpenFileNames(
            this,
            "选择一个或多个TRK文件",
            "data",
            "TRK Files (*.trk);;All Files (*)"
        );

        if (fileNames.isEmpty()) {
            return;
        }

        size_t newlyLoadedTracks = 0;
        size_t newlyLoadedFiles = 0;
        QStringList skippedDuplicates;

        for (const QString& fileName : fileNames) {
            if (fileName.isEmpty()) {
                continue;
            }

            QFileInfo fileInfo(fileName);
            const QString datasetName = fileInfo.completeBaseName();
            if (loadedDatasetNames.contains(datasetName)) {
                skippedDuplicates << datasetName;
                continue;
            }

            statusBar()->showMessage(QString("正在读取 %1 ...").arg(fileInfo.fileName()), 1000);

            auto reader = std::make_unique<DTIFiberLib::TrkFileReader>();
            if (!reader->LoadTractographyFile(fileName.toStdString())) {
                QMessageBox::warning(this, "读取失败",
                    QString("无法读取TRK文件：\n%1\n\n错误信息：%2")
                        .arg(fileName)
                        .arg(QString::fromStdString(reader->GetLastErrorMessage())));
                continue;
            }

            reader->PrintHeaderInfo();
            const size_t appended = appendTracksToRenderer(fileName, *reader);
            if (appended == 0) {
                continue;
            }

            loadedDatasetNames.append(datasetName);
            newlyLoadedTracks += appended;
            newlyLoadedFiles++;
            trkReaders.push_back(std::move(reader));
        }

        if (newlyLoadedFiles == 0) {
            if (!skippedDuplicates.isEmpty()) {
                statusBar()->showMessage(QString("跳过已加载数据集：%1").arg(skippedDuplicates.join(", ")), 5000);
            } else {
                statusBar()->showMessage("没有新的数据集被加载", 3000);
            }
            return;
        }

        glFiberRenderer->setData(*glFiberData);
        glFiberRenderer->setColorMode(DTIFiberLib::FiberColoringMode::DIRECTION_RGB);
        glFiberRenderer->setLineWidth(2.0f);
        glFiberRenderer->setShadingEnabled(toggleShadingAct->isChecked());

        if (lightingSlider) {
            onLightingSliderChanged(lightingSlider->value());
        }
        if (shadowSlider) {
            onShadowSliderChanged(shadowSlider->value());
        }

        float minX, maxX, minY, maxY, minZ, maxZ;
        glFiberRenderer->getBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);
        glWidget->setBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);
        glWidget->update();

        QString summary = QString("新增 %1 个数据集，新加载 %2 条轨迹，累计 %3 条轨迹")
                              .arg(newlyLoadedFiles)
                              .arg(static_cast<qulonglong>(newlyLoadedTracks))
                              .arg(static_cast<qulonglong>(glFiberRenderer->getRenderedTrackCount()));
        if (!skippedDuplicates.isEmpty()) {
            summary += QString("；已忽略：%1").arg(skippedDuplicates.join(", "));
        }
        statusBar()->showMessage(summary, 6000);

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误",
            QString("读取TRK文件时发生异常：%1").arg(e.what()));
        statusBar()->showMessage("读取TRK文件异常", 3000);
    }
}
