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
#include <QSignalBlocker>
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
    , sagittalSlider(nullptr)
    , coronalSlider(nullptr)
    , axialSlider(nullptr)
{
    setWindowTitle("DTI Fiber Viewer - OpenGL");
    resize(800, 600);
    niftiExtentMin.fill(0);
    niftiSpacing.fill(1.0);

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

    openNiftiAct = new QAction("打开NIfTI文件(&N)", this);
    openNiftiAct->setStatusTip("加载并显示NIfTI体数据切片");
    connect(openNiftiAct, &QAction::triggered, this, &MainWindow::openNiftiFile);

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

void MainWindow::onSagittalSliceChanged(int value)
{
    applySliceSlider(DTIFiberLib::SliceAxis::Sagittal, value);
}

void MainWindow::onCoronalSliceChanged(int value)
{
    applySliceSlider(DTIFiberLib::SliceAxis::Coronal, value);
}

void MainWindow::onAxialSliceChanged(int value)
{
    applySliceSlider(DTIFiberLib::SliceAxis::Axial, value);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(openTrkAct);
    fileMenu->addAction(openNiftiAct);
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
    fileToolBar->addAction(openNiftiAct);
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

    QWidget* sagittalWidget = new QWidget(fileToolBar);
    QHBoxLayout* sagittalLayout = new QHBoxLayout(sagittalWidget);
    sagittalLayout->setContentsMargins(6, 0, 6, 0);
    QLabel* sagittalLabel = new QLabel("矢状", sagittalWidget);
    sagittalSlider = new QSlider(Qt::Horizontal, sagittalWidget);
    sagittalSlider->setRange(0, 0);
    sagittalSlider->setValue(0);
    sagittalSlider->setEnabled(false);
    sagittalSlider->setFixedWidth(120);
    sagittalSlider->setToolTip("矢状面切片索引");
    sagittalLayout->addWidget(sagittalLabel);
    sagittalLayout->addWidget(sagittalSlider);
    fileToolBar->addWidget(sagittalWidget);
    connect(sagittalSlider, &QSlider::valueChanged, this, &MainWindow::onSagittalSliceChanged);

    QWidget* coronalWidget = new QWidget(fileToolBar);
    QHBoxLayout* coronalLayout = new QHBoxLayout(coronalWidget);
    coronalLayout->setContentsMargins(6, 0, 6, 0);
    QLabel* coronalLabel = new QLabel("冠状", coronalWidget);
    coronalSlider = new QSlider(Qt::Horizontal, coronalWidget);
    coronalSlider->setRange(0, 0);
    coronalSlider->setValue(0);
    coronalSlider->setEnabled(false);
    coronalSlider->setFixedWidth(120);
    coronalSlider->setToolTip("冠状面切片索引");
    coronalLayout->addWidget(coronalLabel);
    coronalLayout->addWidget(coronalSlider);
    fileToolBar->addWidget(coronalWidget);
    connect(coronalSlider, &QSlider::valueChanged, this, &MainWindow::onCoronalSliceChanged);

    QWidget* axialWidget = new QWidget(fileToolBar);
    QHBoxLayout* axialLayout = new QHBoxLayout(axialWidget);
    axialLayout->setContentsMargins(6, 0, 6, 0);
    QLabel* axialLabel = new QLabel("轴向", axialWidget);
    axialSlider = new QSlider(Qt::Horizontal, axialWidget);
    axialSlider->setRange(0, 0);
    axialSlider->setValue(0);
    axialSlider->setEnabled(false);
    axialSlider->setFixedWidth(120);
    axialSlider->setToolTip("轴向切片索引");
    axialLayout->addWidget(axialLabel);
    axialLayout->addWidget(axialSlider);
    fileToolBar->addWidget(axialWidget);
    connect(axialSlider, &QSlider::valueChanged, this, &MainWindow::onAxialSliceChanged);
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

void MainWindow::openNiftiFile()
{
    if (!glFiberRenderer) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择一个NIfTI文件",
        "data",
        "NIfTI Files (*.nii *.nii.gz);;All Files (*)");

    if (fileName.isEmpty()) {
        return;
    }

    auto volume = std::make_shared<DTIFiberLib::NiftiVolume>();
    if (!volume->loadFromFile(fileName.toStdString())) {
        QMessageBox::warning(this,
                             "NIfTI加载失败",
                             QString("无法加载NIfTI文件：\n%1").arg(fileName));
        return;
    }

    niftiVolume = volume;
    const auto dims = volume->getDimensions();
    const auto spacing = volume->getSpacing();
    niftiSpacing = spacing;
    for (size_t axis = 0; axis < niftiExtentMin.size(); ++axis) {
        niftiExtentMin[axis] = volume->getExtentMin(static_cast<DTIFiberLib::SliceAxis>(axis));
    }

    glFiberRenderer->setNiftiVolume(niftiVolume);
    glFiberRenderer->setSliceOpacity(0.5f);

    auto configureSlider = [&](QSlider* slider, DTIFiberLib::SliceAxis axis) {
        if (!slider || !niftiVolume) {
            return;
        }
        const int sliceCount = niftiVolume->getSliceCount(axis);
        slider->setEnabled(sliceCount > 1);
        if (sliceCount > 0) {
            slider->setRange(0, sliceCount - 1);
        } else {
            slider->setRange(0, 0);
        }

        const int defaultValue = sliceCount > 0 ? sliceCount / 2 : 0;
        {
            QSignalBlocker blocker(slider);
            slider->setValue(defaultValue);
        }

        if (sliceCount > 0) {
            applySliceSlider(axis, defaultValue);
        }
    };

    configureSlider(sagittalSlider, DTIFiberLib::SliceAxis::Sagittal);
    configureSlider(coronalSlider, DTIFiberLib::SliceAxis::Coronal);
    configureSlider(axialSlider, DTIFiberLib::SliceAxis::Axial);

    float minX, maxX, minY, maxY, minZ, maxZ;
    glFiberRenderer->getBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);
    if (glWidget) {
        glWidget->setBoundingBox(minX, maxX, minY, maxY, minZ, maxZ);
        glWidget->update();
    }

    QFileInfo info(fileName);
    statusBar()->showMessage(
        QString("已加载NIfTI：%1 | 体素 %2×%3×%4 | spacing %.2f / %.2f / %.2f mm")
            .arg(info.fileName())
            .arg(dims[0])
            .arg(dims[1])
            .arg(dims[2])
            .arg(spacing[0], 0, 'f', 2)
            .arg(spacing[1], 0, 'f', 2)
            .arg(spacing[2], 0, 'f', 2),
        6000);
}

QString MainWindow::sliceAxisLabel(DTIFiberLib::SliceAxis axis) const
{
    switch (axis) {
    case DTIFiberLib::SliceAxis::Sagittal: return "矢状";
    case DTIFiberLib::SliceAxis::Coronal: return "冠状";
    case DTIFiberLib::SliceAxis::Axial: return "轴向";
    default: return QStringLiteral("未知");
    }
}

void MainWindow::applySliceSlider(DTIFiberLib::SliceAxis axis, int sliderValue)
{
    if (!glFiberRenderer || !niftiVolume) {
        return;
    }

    const int voxelIndex = niftiVolume->sliderToVoxel(axis, sliderValue);
    glFiberRenderer->setSliceIndex(axis, voxelIndex);

    if (glWidget) {
        glWidget->update();
    }

    const auto spacing = niftiVolume->getSpacing();
    const int axisIdx = static_cast<int>(axis);
    const double locationMM = spacing[axisIdx] * (voxelIndex - niftiExtentMin[axisIdx]);

    QSlider* slider = nullptr;
    switch (axis) {
    case DTIFiberLib::SliceAxis::Sagittal: slider = sagittalSlider; break;
    case DTIFiberLib::SliceAxis::Coronal: slider = coronalSlider; break;
    case DTIFiberLib::SliceAxis::Axial: slider = axialSlider; break;
    default: break;
    }

    if (slider) {
        slider->setToolTip(QString("%1索引 %2 | %.2f mm")
                               .arg(sliceAxisLabel(axis))
                               .arg(sliderValue)
                               .arg(locationMM, 0, 'f', 2));
    }
}
