# C++版本TRK文件可视化实现计划

## 项目概述

基于TypeScript版本的Neuroglancer-Tractography项目，开发一个C++版本的.trk文件解析和可视化库。重点是**实用性优先**，快速实现能够读取和渲染.trk文件的基础功能。

## 技术参考和验证

- **主要参考**: [Neuroglancer-Tractography](https://github.com/shrutivarade/Neuroglancer-Tractography) TypeScript实现
- **测试文件**: `data/AF_L.trk` (实际神经纤维束数据)
- **渲染引擎**: VTK 8.2  
- **UI框架**: Qt 5.14
- **开发策略**: 参考TypeScript代码逻辑，直接翻译为C++实现

## 修正后的实现策略

### ✅ **已验证的事实**
1. TypeScript项目能成功读取.trk文件
2. 我们有实际的AF_L.trk测试文件
3. .trk文件确实以"TRACK"开头，包含1000字节头部
4. 文件包含大量轨迹坐标数据

### 🎯 **实用优先的目标**
1. **能读取AF_L.trk文件** - 验证解析正确性
2. **能渲染基本的纤维束** - 在VTK中显示轨迹线条
3. **基础交互功能** - 缩放、旋转、颜色调节
4. **代码简洁可维护** - 避免过度设计

## 一、简化的TRK文件读取器 (第一阶段 - 1周)

### 1.1 最小可行的头部结构

```cpp
// header/TrkFileReader.h
namespace DTIFiberLib {
    
    // 轨迹学文件头部结构 - 只读取必要字段
    struct TractographyHeader {
        char magic[6];           // "TRACK\0"
        uint16_t dim[3];         // 体积维度
        float voxel_size[3];     // 体素大小
        float origin[3];         // 原点
        uint16_t n_scalars;      // 标量数
        // ... 跳过中间复杂字段 ...
        uint32_t n_count;        // 轨迹数 (偏移988)
        uint32_t version;        // 版本 (偏移992) 
        uint32_t hdr_size;       // 头部大小 (偏移996)
    };
    
    // 轨迹点结构
    struct TrackPoint {
        float x, y, z;
        std::vector<float> scalars;  // 可选标量
    };
    
    // 单条轨迹
    using FiberTrack = std::vector<TrackPoint>;
}
```

### 1.2 实用的读取器类

```cpp
// 参考TypeScript实现的轨迹文件读取器
class TrkFileReader {
public:
    SimpleTrkReader();
    ~SimpleTrkReader();
    
    // 核心功能
    bool LoadFile(const std::string& filename);
    bool IsValid() const;
    
    // 获取数据 - 直接用于VTK渲染
    size_t GetTrackCount() const;
    FiberTrack GetTrack(size_t index) const;
    std::vector<FiberTrack> GetAllTracks() const;
    
    // 头部信息
    TractographyHeader GetHeader() const;
    
    // 调试信息
    void PrintHeader() const;
    std::string GetStatusMessage() const;
    
private:
    bool ParseTrkHeader();
    bool ExtractFiberTracks();
    bool ValidateFile();
    
    std::ifstream file;
    TractographyHeader m_tractographyHeader;
    std::vector<FiberTrack> m_fiberTracks;
    bool m_isValidFile;
    std::string m_lastErrorMessage;
};
```

### 1.3 基于AF_L.trk的实际读取流程

#### 1.3.1 实际验证的读取步骤

```cpp
// 基于实际AF_L.trk文件验证的读取流程
bool TrkFileReader::LoadTractographyFile(const std::string& filename) {
    // 1. 打开二进制文件
    file.open(filename, std::ios::binary);
    if (!file.is_open()) {
        statusMessage = "无法打开文件: " + filename;
        return false;
    }
    
    // 2. 读取关键头部字段
    if (!ParseTrkHeader()) {
        return false;
    }
    
    // 3. 读取轨迹数据
    if (!ExtractFiberTracks()) {
        return false;
    }
    
    m_isValidFile = true;
    m_lastErrorMessage = "成功读取 " + std::to_string(m_fiberTracks.size()) + " 条轨迹";
    return true;
}

bool TrkFileReader::ParseTrkHeader() {
    // 读取魔术字符串
    file.read(m_tractographyHeader.magic, 6);
    if (std::strncmp(m_tractographyHeader.magic, "TRACK", 5) != 0) {
        m_lastErrorMessage = "文件格式错误：不是有效的TRK文件";
        return false;
    }
    
    // 读取维度信息
    file.read(reinterpret_cast<char*>(m_tractographyHeader.dim), sizeof(uint16_t) * 3);
    file.read(reinterpret_cast<char*>(m_tractographyHeader.voxel_size), sizeof(float) * 3);
    file.read(reinterpret_cast<char*>(m_tractographyHeader.origin), sizeof(float) * 3);
    file.read(reinterpret_cast<char*>(&m_tractographyHeader.n_scalars), sizeof(uint16_t));
    
    // 跳转到关键字段位置
    file.seekg(988, std::ios::beg);  // n_count位置
    file.read(reinterpret_cast<char*>(&m_tractographyHeader.n_count), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&m_tractographyHeader.version), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&m_tractographyHeader.hdr_size), sizeof(uint32_t));
    
    // 验证头部大小
    if (m_tractographyHeader.hdr_size != 1000) {
        m_lastErrorMessage = "头部大小错误，可能需要字节序转换";
        // 尝试字节序转换
        // TODO: 实现字节序转换
    }
    
    return true;
}

bool TrkFileReader::ExtractFiberTracks() {
    // 跳转到数据开始位置
    file.seekg(1000, std::ios::beg);
    
    m_fiberTracks.clear();
    
    // 读取轨迹数据
    while (!file.eof()) {
        // 读取点数
        uint32_t n_points;
        file.read(reinterpret_cast<char*>(&n_points), sizeof(uint32_t));
        
        if (file.eof() || n_points == 0 || n_points > 10000) {
            break;  // 文件结束或数据异常
        }
        
        FiberTrack track;
        track.reserve(n_points);
        
        // 读取所有点
        for (uint32_t i = 0; i < n_points; ++i) {
            TrackPoint point;
            
            // 读取坐标
            file.read(reinterpret_cast<char*>(&point.x), sizeof(float));
            file.read(reinterpret_cast<char*>(&point.y), sizeof(float));
            file.read(reinterpret_cast<char*>(&point.z), sizeof(float));
            
            // 读取标量（如果有）
            if (m_tractographyHeader.n_scalars > 0) {
                point.scalars.resize(m_tractographyHeader.n_scalars);
                file.read(reinterpret_cast<char*>(point.scalars.data()), 
                         sizeof(float) * m_tractographyHeader.n_scalars);
            }
            
            track.push_back(point);
        }
        
        m_fiberTracks.push_back(track);
        
        // 跳过轨迹属性（如果有）
        if (m_tractographyHeader.n_properties > 0) {
            file.seekg(sizeof(float) * m_tractographyHeader.n_properties, std::ios::cur);
        }
    }
    
    return true;
}
```

#### 1.3.2 调试和验证功能

```cpp
void TrkFileReader::PrintHeaderInfo() const {
    std::cout << "=== TRK文件头部信息 ===" << std::endl;
    std::cout << "魔术字符串: " << std::string(m_tractographyHeader.magic, 5) << std::endl;
    std::cout << "维度: " << m_tractographyHeader.dim[0] << " x " << m_tractographyHeader.dim[1] << " x " << m_tractographyHeader.dim[2] << std::endl;
    std::cout << "体素大小: " << m_tractographyHeader.voxel_size[0] << " x " << m_tractographyHeader.voxel_size[1] << " x " << m_tractographyHeader.voxel_size[2] << std::endl;
    std::cout << "轨迹数量: " << m_tractographyHeader.n_count << std::endl;
    std::cout << "版本: " << m_tractographyHeader.version << std::endl;
    std::cout << "标量数: " << m_tractographyHeader.n_scalars << std::endl;
    std::cout << "实际读取轨迹数: " << m_fiberTracks.size() << std::endl;
}
```

## 二、简化的VTK渲染器 (第二阶段 - 1周)

### 2.1 直接用于VTK的渲染器

```cpp
// header/FiberBundleRenderer.h
namespace DTIFiberLib {
    
    class FiberBundleRenderer {
    public:
        SimpleFiberRenderer();
        ~SimpleFiberRenderer();
        
        // 设置数据
        void SetFiberTracks(const std::vector<FiberTrack>& tracks);
        void SetTrackSubset(const std::vector<FiberTrack>& tracks, size_t maxTracks = 1000);
        
        // 渲染设置
        void SetLineColor(float r, float g, float b);
        void SetLineWidth(float width);
        void SetOpacity(float opacity);
        
        // 颜色模式
        enum class FiberColoringMode {
            SOLID_COLOR,      // 单色
            DIRECTION_RGB,    // 按方向RGB着色
            RANDOM_COLORS     // 随机颜色
        };
        void SetColoringMode(FiberColoringMode mode);
        
        // VTK集成
        vtkActor* GetActor();
        void UpdateVTK();
        
        // 统计信息
        size_t GetRenderedTrackCount() const;
        size_t GetTotalPointCount() const;
        
    private:
        void BuildVTKPipeline();
        void SetDirectionColors();
        void SetSolidColors();
        void SetRandomColors();
        
        std::vector<FiberTrack> m_fiberTracks;
        FiberColoringMode m_coloringMode;
        float lineColor[3];
        float lineWidth;
        float opacity;
        
        // VTK对象
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> lines;
        vtkSmartPointer<vtkUnsignedCharArray> colors;
    };
}
```

### 2.2 主程序集成

```cpp
// 在MainWindow中集成TRK文件加载和渲染
class MainWindow : public QMainWindow {
    // ... 现有代码 ...
    
private slots:
    void openTrkFile();          // 新增：打开TRK文件
    void setLineWidth(double width);     // 新增：调节线宽
    void setColorMode(int mode);         // 新增：设置颜色模式
    void showTrackInfo();               // 新增：显示轨迹信息
    
private:
    // 新增成员
    std::unique_ptr<TrkFileReader> trkReader;
    std::unique_ptr<FiberBundleRenderer> fiberRenderer;
    
    // UI控件
    QSlider* lineWidthSlider;
    QComboBox* colorModeCombo;
    QLabel* trackInfoLabel;
    
    void setupTrkControls();    // 新增：设置TRK相关控件
    void updateTrackDisplay();  // 新增：更新轨迹显示
};

void MainWindow::openTrkFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this, "打开TRK文件", "", "TRK Files (*.trk)");
    
    if (!fileName.isEmpty()) {
        trkReader = std::make_unique<TrkFileReader>();
        
        if (trkReader->LoadTractographyFile(fileName.toStdString())) {
            // 显示文件信息
            trkReader->PrintHeaderInfo();
            
            // 设置渲染数据
            auto tracks = trkReader->GetAllTracks();
            fiberRenderer->SetTrackSubset(tracks, 1000);  // 限制显示数量
            fiberRenderer->RenderFiberBundle();
            
            // 添加到VTK渲染器
            dtiRenderer->GetRenderer()->AddActor(fiberRenderer->GetActor());
            dtiRenderer->ResetCamera();
            dtiRenderer->Render();
            
            updateTrackDisplay();
        } else {
            QMessageBox::warning(this, "错误", 
                QString("无法读取TRK文件: %1").arg(trkReader->GetLastErrorMessage().c_str()));
        }
    }
}
```

## 三、修正的实现时间表

### 🚀 **第1周：TRK文件读取验证**
- [x] 分析AF_L.trk文件结构  
- [ ] 实现TrkFileReader类
- [ ] 测试读取AF_L.trk文件
- [ ] 验证头部信息解析
- [ ] 验证轨迹数据读取

**验收标准**: 能够成功读取AF_L.trk文件并输出正确的头部信息和轨迹数量

### 🎨 **第2周：基础VTK渲染**
- [ ] 实现FiberBundleRenderer类
- [ ] 集成到DTIFiberViewer主程序
- [ ] 实现基础线条渲染
- [ ] 添加颜色和线宽控制
- [ ] 测试渲染性能

**验收标准**: 能够在VTK窗口中显示AF_L.trk的纤维束，支持基本的颜色和线宽调节

### 🔧 **第3周：UI集成和优化**
- [ ] 添加文件打开对话框
- [ ] 添加渲染控制面板
- [ ] 实现轨迹信息显示
- [ ] 优化大数据集渲染性能
- [ ] 添加错误处理和用户提示

**验收标准**: 完整的用户界面，能够方便地加载和查看.trk文件

### 📊 **关键里程碑**

#### 里程碑1：文件读取成功 (第1周结束)
```bash
# 期望输出
=== TRK文件头部信息 ===
魔术字符串: TRACK
维度: 128 x 128 x 60
体素大小: 2.0 x 2.0 x 2.0
轨迹数量: 1000
版本: 2
标量数: 0
实际读取轨迹数: 1000
```

#### 里程碑2：基础渲染成功 (第2周结束)
- VTK窗口显示纤维束
- 支持鼠标交互（缩放、旋转）
- 基础颜色控制

#### 里程碑3：完整功能 (第3周结束)
- 用户友好的界面
- 文件加载对话框
- 渲染参数控制
- 性能优化

## 四、UI集成计划 (第四阶段)

### 4.1 主窗口功能扩展

```cpp
// 新增功能到MainWindow
class MainWindow : public QMainWindow {
    // ... 现有代码 ...
    
private slots:
    // 文件操作
    void openTrkFile();
    void saveTrkFile();
    void exportToVTK();
    
    // 可视化控制
    void setColorMode(int mode);
    void setRenderMode(int mode);
    void adjustLineWidth(double width);
    void adjustOpacity(double opacity);
    
    // 过滤操作
    void filterByLength();
    void filterByROI();
    void resetFilters();
    
    // 统计显示
    void showStatistics();
    void showFiberInfo(size_t index);
    
private:
    // 新增成员
    std::unique_ptr<TrkFileReader> trkReader;
    std::unique_ptr<FiberBundle> fiberBundle;
    std::unique_ptr<FiberBundleRenderer> fiberRenderer;
    
    // UI控件
    QSlider* lineWidthSlider;
    QSlider* opacitySlider;
    QComboBox* colorModeCombo;
    QComboBox* renderModeCombo;
    QDockWidget* statsDock;
    QTableWidget* statsTable;
};
```

### 4.2 UI布局设计

```
主窗口布局:
┌─────────────────────────────────────────────────┐
│  菜单栏: 文件 | 视图 | 过滤 | 工具 | 帮助          │
├─────────────────────────────────────────────────┤
│  工具栏: [打开] [保存] | [线条] [管道] | [颜色]    │
├──────────────────┬──────────────────────────────┤
│                  │                              │
│   控制面板        │      VTK渲染窗口             │
│                  │                              │
│ - 颜色模式       │                              │
│ - 渲染模式       │                              │
│ - 线宽          │                              │
│ - 透明度        │                              │
│ - 过滤器        │                              │
│                  │                              │
├──────────────────┴──────────────────────────────┤
│  统计信息: 纤维数: 10000 | 平均长度: 45.2mm       │
└─────────────────────────────────────────────────┘
```

## 五、实现时间表

### 第1周：基础架构
- [ ] 创建TrkFileFormat.h头文件定义
- [ ] 实现TrkFileReader基础框架
- [ ] 编写单元测试框架
- [ ] 准备测试.trk文件

### 第2周：文件解析
- [ ] 实现二进制文件读取
- [ ] 处理字节序和数据类型转换
- [ ] 实现坐标系转换
- [ ] 验证文件解析正确性

### 第3周：数据结构
- [ ] 实现FiberTrack类
- [ ] 实现FiberBundle类
- [ ] 添加过滤和统计功能
- [ ] 性能优化（内存管理）

### 第4周：VTK渲染
- [ ] 实现基础线条渲染
- [ ] 添加颜色映射功能
- [ ] 实现管道和带状渲染
- [ ] 添加LOD支持

### 第5周：UI集成
- [ ] 扩展MainWindow功能
- [ ] 添加控制面板
- [ ] 实现文件对话框
- [ ] 添加统计信息显示

### 第6周：优化和测试
- [ ] 性能优化（大数据集）
- [ ] 完善错误处理
- [ ] 编写用户文档
- [ ] 发布测试版本

## 六、性能优化策略

### 6.1 内存优化
- 使用内存映射文件读取大型.trk文件
- 实现按需加载策略
- 使用对象池管理FiberTrack实例

### 6.2 渲染优化
- 实现视锥体剔除
- 使用LOD技术减少远处纤维细节
- 批量渲染相似纤维束
- 使用VTK的GPU加速特性

### 6.3 并行处理
- 使用OpenMP并行化过滤操作
- 多线程文件加载
- 异步统计计算

## 七、测试策略

### 7.1 单元测试
- TrkHeader结构体大小和对齐
- 文件读取正确性
- 坐标变换精度
- 过滤算法准确性

### 7.2 集成测试
- 完整的文件加载到渲染流程
- UI交互响应
- 内存泄漏检测
- 性能基准测试

### 7.3 兼容性测试
- 不同版本的.trk文件
- 不同大小的数据集
- 不同的坐标系配置

## 八、参考资源

### 代码参考
- [Neuroglancer-Tractography (TypeScript)](https://github.com/shrutivarade/Neuroglancer-Tractography)
- [nibabel Python库](https://github.com/nipy/nibabel)
- [TrackVis官方文档](https://trackvis.org/docs/)

### 算法参考
- 纤维束聚类：QuickBundles算法
- 颜色映射：Directional Color Encoding
- 过滤算法：ROI-based Selection

### 工具库
- VTK 8.2：3D渲染
- Qt 5.14：UI框架
- OpenMP：并行计算
- Eigen3：矩阵运算

---

*创建日期：2025年9月26日*
*版本：1.0*