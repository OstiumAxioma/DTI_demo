# OpenGL渲染实现计划

## 一、库准备情况

### 已有库（齐全）
- ✅ **GLAD** (OpenGL 4.6 Core Profile) - OpenGL函数加载器
- ✅ **GLFW3** (窗口管理) - 独立窗口创建库
- ✅ **KHR** (Khronos平台头文件) - OpenGL平台定义
- ✅ **Qt的OpenGL支持** (QOpenGLWidget) - Qt集成的OpenGL

### GLEW不需要的原因
**不需要GLEW**，因为：
1. **GLAD已经存在** - GLAD和GLEW功能相同，都是OpenGL加载器，二选一即可
2. **Qt提供了QOpenGLFunctions** - Qt自带OpenGL函数加载机制
3. **GLAD更现代** - GLAD是更新的解决方案，支持按需生成

### 建议添加的库
- **GLM** (OpenGL Mathematics) - 用于矩阵和向量运算
- 可通过vcpkg或手动下载：`vcpkg install glm`

## 二、架构设计

### 2.1 技术选择
由于已经使用Qt，建议使用**QOpenGLWidget**而不是GLFW：
- Qt集成更好，无需管理额外窗口
- 可以直接嵌入现有UI
- 事件处理更简单
- 但保留GLAD用于OpenGL 4.6高级特性

### 2.2 类结构设计

```cpp
namespace DTIFiberLib {

// OpenGL着色器管理类
class GLShaderProgram {
public:
    GLShaderProgram();
    ~GLShaderProgram();

    bool loadFromFile(const std::string& vertPath, const std::string& fragPath);
    bool loadFromString(const char* vertSource, const char* fragSource);
    void use();

    void setUniform(const char* name, const glm::mat4& matrix);
    void setUniform(const char* name, int value);
    void setUniform(const char* name, float value);

private:
    GLuint compileShader(const char* source, GLenum type);
    GLuint m_programID;
};

// OpenGL纤维束渲染器 (嵌入Qt，不是独立Widget)
class GLFiberRenderer {
public:
    GLFiberRenderer();
    ~GLFiberRenderer();

    // 数据接口
    void setTracks(const std::vector<FiberTrack>& tracks);
    void setColorMode(FiberColoringMode mode);
    void setLineWidth(float width);

    // 渲染控制
    void initialize();  // 初始化OpenGL资源
    void render(const glm::mat4& mvpMatrix);  // 渲染
    void cleanup();     // 清理资源

    // 性能控制
    void setLODEnabled(bool enable);
    void setMaxPointsPerTrack(size_t maxPoints);

    size_t getRenderedTrackCount() const { return m_renderedTrackCount; }
    size_t getTotalPointCount() const { return m_totalPointCount; }

private:
    void uploadToGPU();
    void buildVertexData();

    GLuint m_VAO, m_VBO;
    std::unique_ptr<GLShaderProgram> m_shader;

    std::vector<FiberTrack> m_tracks;
    FiberColoringMode m_colorMode;
    float m_lineWidth;

    size_t m_renderedTrackCount;
    size_t m_totalPointCount;
    bool m_lodEnabled;
    size_t m_maxPointsPerTrack;
};

} // namespace DTIFiberLib
```

**主窗口OpenGL Widget:**
```cpp
// 在src/mainwindow.h中
class GLFiberWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_6_Core {
    Q_OBJECT
public:
    explicit GLFiberWidget(QWidget* parent = nullptr);
    ~GLFiberWidget();

    void setFiberRenderer(DTIFiberLib::GLFiberRenderer* renderer);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 鼠标交互
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    DTIFiberLib::GLFiberRenderer* m_fiberRenderer;
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    glm::mat4 m_modelMatrix;

    // 相机控制
    float m_cameraDistance;
    glm::vec3 m_cameraRotation;
};
```

### 2.3 VTK与OpenGL职责划分（明确）

| 功能 | VTK（VTKSliceViewer） | OpenGL（GLFiberRenderer） | 说明 |
|------|---------------------|--------------------------|------|
| **纤维束3D渲染** | ❌ 不再负责 | ✅ 唯一负责 | OpenGL取代VTK，解决内存问题 |
| **TRK文件读取** | ✅ TrkFileReader | ❌ | 使用VTK的IO库 |
| **医学图像切片** | ✅ 负责 | ❌ | 三视图显示MRI/CT |
| **体积渲染** | ✅ 负责 | ❌ | 3D医学图像体渲染 |
| **DICOM/NIfTI** | ✅ 负责 | ❌ | 医学影像格式读取 |
| **图像滤波/处理** | ✅ 负责 | ❌ | VTK丰富的算法库 |
| **表面重建** | ✅ 负责 | ❌ | Marching Cubes等 |

**关键点：纤维束渲染完全由OpenGL负责，VTK只负责医学图像相关功能**

## 三、实现步骤

### 第1步：重构文件架构

#### 1.1 现有文件重命名和职责调整

**静态库 (static/)：**
- `TrkFileReader.h/cpp` - **保留**，负责TRK文件读取
- `FiberBundleRenderer.h/cpp` - **删除**（不再需要VTK渲染纤维束）
- `DTIRenderer.h/cpp` - **重命名为** `VTKSliceViewer.h/cpp`
  * **职责：仅负责医学图像的切片显示、体积渲染**
  * 显示MRI/CT等医学图像的三视图（轴位、冠状位、矢状位）
  * 不涉及纤维束渲染
- `DTIFiberLib.h` - **更新** 统一入口头文件

**新增OpenGL渲染器 (static/)：**
- `GLFiberRenderer.h/cpp` - **新建** OpenGL纤维束渲染器
  * **职责：专门负责3D纤维束渲染（取代VTK方案）**
  * 高性能，可处理400万+轨迹
  * 使用GPU加速，支持大规模数据
  * 实现LOD、视锥体剔除等优化
- `GLShaderProgram.h/cpp` - **新建** 着色器管理类
  * 管理OpenGL顶点/片段着色器
  * 封装着色器编译和uniform设置

#### 1.2 新文件结构
```
DTI_demo/
├── static/                        # 静态库
│   ├── header/
│   │   ├── TrkFileReader.h        # 保留 - TRK文件读取
│   │   ├── VTKSliceViewer.h       # 重命名自DTIRenderer.h - 医学图像切片显示
│   │   ├── GLFiberRenderer.h      # 新建 - OpenGL纤维束渲染器
│   │   ├── GLShaderProgram.h      # 新建 - 着色器管理
│   │   └── DTIFiberLib.h          # 更新 - 统一入口
│   ├── src/
│   │   ├── TrkFileReader.cpp
│   │   ├── VTKSliceViewer.cpp     # 重命名自DTIRenderer.cpp
│   │   ├── GLFiberRenderer.cpp    # 新建
│   │   └── GLShaderProgram.cpp    # 新建
│   └── shaders/                   # 新建目录
│       ├── fiber.vert
│       ├── fiber.frag
│       └── fiber.geom (可选)
├── src/                           # 主程序
│   ├── main.cpp
│   ├── mainwindow.h
│   ├── mainwindow.cpp
│   └── mainwindow.ui

注：FiberBundleRenderer.h/cpp 已删除，不再使用VTK渲染纤维束
```

### 第2步：实现着色器

**顶点着色器 (fiber.vert):**
```glsl
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aDirection;  // 用于颜色计算

out vec3 FragColor;

uniform mat4 uMVPMatrix;
uniform int uColorMode;  // 0=direction, 1=solid, 2=random

void main() {
    gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
    
    // 根据方向计算RGB颜色
    if (uColorMode == 0) {
        FragColor = abs(normalize(aDirection));
    } else {
        FragColor = vec3(1.0, 0.0, 0.0);  // 默认红色
    }
}
```

**片段着色器 (fiber.frag):**
```glsl
#version 460 core
in vec3 FragColor;
out vec4 FragmentColor;

uniform float uOpacity;

void main() {
    FragmentColor = vec4(FragColor, uOpacity);
}
```

### 第3步：数据上传策略

```cpp
class FiberGPUData {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 direction;
    };
    
    void uploadToGPU(const std::vector<FiberTrack>& tracks) {
        // 1. 预计算总顶点数
        size_t totalVertices = 0;
        for (const auto& track : tracks) {
            totalVertices += track.size();
        }
        
        // 2. 分批上传（每批最多100万顶点）
        const size_t BATCH_SIZE = 1000000;
        if (totalVertices > BATCH_SIZE) {
            // 实现分批渲染
        }
        
        // 3. 使用持久映射缓冲区
        glBufferStorage(GL_ARRAY_BUFFER, size, nullptr, 
                       GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    }
};
```

### 第4步：性能优化技术

#### 4.1 实例化渲染
```cpp
// 对于相似的纤维束，使用实例化渲染
glDrawArraysInstanced(GL_LINE_STRIP, 0, vertexCount, instanceCount);
```

#### 4.2 视锥体剔除
```cpp
bool isInFrustum(const BoundingBox& bbox, const Frustum& frustum) {
    // 实现AABB-Frustum相交测试
}
```

#### 4.3 LOD系统
```cpp
enum LODLevel {
    LOD_HIGH = 0,    // 100% 顶点
    LOD_MEDIUM = 1,  // 50% 顶点
    LOD_LOW = 2      // 25% 顶点
};

LODLevel calculateLOD(float distance) {
    if (distance < 100.0f) return LOD_HIGH;
    if (distance < 500.0f) return LOD_MEDIUM;
    return LOD_LOW;
}
```

### 第5步：与Qt集成

```cpp
// mainwindow.h
#include "DTIFiberLib.h"
#include <QOpenGLWidget>

class GLFiberWidget;  // 前向声明

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();

private:
    void setupUI();

    Ui::MainWindow *ui;

    // OpenGL渲染
    GLFiberWidget* glWidget;                          // OpenGL渲染窗口
    DTIFiberLib::GLFiberRenderer* glFiberRenderer;    // OpenGL渲染器

    // VTK相关（保留用于切片等功能）
    QVTKOpenGLWidget* vtkWidget;                      // VTK窗口（可选）

    // 数据
    std::unique_ptr<DTIFiberLib::TrkFileReader> trkReader;
};

// mainwindow.cpp 实现
void MainWindow::setupUI() {
    // 创建中央widget
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);

    // 创建OpenGL渲染窗口
    glWidget = new GLFiberWidget(this);
    layout->addWidget(glWidget);

    // 创建渲染器
    glFiberRenderer = new DTIFiberLib::GLFiberRenderer();
    glWidget->setFiberRenderer(glFiberRenderer);

    setCentralWidget(centralWidget);
}

void MainWindow::onOpenFile() {
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("打开TRK文件"), "", tr("TRK Files (*.trk)"));

    if (!fileName.isEmpty()) {
        trkReader = std::make_unique<DTIFiberLib::TrkFileReader>(fileName.toStdString());

        if (trkReader->LoadTrkFile()) {
            const auto& tracks = trkReader->GetAllTracks();

            // 使用OpenGL渲染
            glFiberRenderer->setTracks(tracks);
            glFiberRenderer->setColorMode(DTIFiberLib::FiberColoringMode::DIRECTION_RGB);
            glWidget->update();

            statusBar()->showMessage(QString("成功加载 %1 条纤维束")
                .arg(tracks.size()));
        }
    }
}
```

## 四、内存管理策略

### 4.1 避免VTK内存错误的关键
1. **不复制数据** - 直接从TrkFileReader传递指针
2. **流式上传** - 分批将数据上传到GPU
3. **使用索引缓冲** - 共享顶点减少内存

### 4.2 内存限制检查
```cpp
bool checkMemoryLimit(size_t vertexCount) {
    size_t bytesNeeded = vertexCount * sizeof(Vertex);
    
    // OpenGL纹理/缓冲区单个对象限制通常为2GB
    const size_t GL_MAX_BUFFER_SIZE = 2147483648;  // 2GB
    
    if (bytesNeeded > GL_MAX_BUFFER_SIZE) {
        // 需要分批处理
        return false;
    }
    return true;
}
```

## 五、性能预期

### 对比表

| 指标 | 当前VTK实现 | OpenGL实现目标 |
|------|------------|---------------|
| 最大轨迹数 | ~10万（内存限制） | 400万+ |
| 帧率 | 15-30 FPS | 60 FPS |
| 内存占用 | 高（全部在RAM） | 低（数据在GPU） |
| 加载时间 | 慢（构建VTK管道） | 快（直接上传） |
| LOD支持 | 无 | 动态LOD |
| 着色器效果 | 受限 | 完全自定义 |

## 六、迁移执行步骤（渐进式）

### 阶段0：准备工作（清理现有文件）
1. **删除** VTK纤维束渲染相关文件：
   - 删除 `FiberBundleRenderer.h/cpp`（不再需要）
2. **重命名** VTK窗口管理器：
   - `DTIRenderer.h/cpp` → `VTKSliceViewer.h/cpp`
3. 更新所有引用这些文件的地方
4. 更新CMakeLists.txt
5. 更新DTIFiberLib.h统一入口

### 阶段1：基础OpenGL渲染
1. 创建OpenGL渲染器类：
   - 创建 `GLShaderProgram.h/cpp`
   - 创建 `GLFiberRenderer.h/cpp`
   - 创建 `GLFiberWidget` (在src/)
2. 实现基本的线条渲染
3. 实现方向RGB着色
4. 替换mainwindow中的渲染逻辑（从VTK切换到OpenGL）

### 阶段2：功能完善
1. 实现完整的纤维束渲染功能：
   - 相机控制（旋转、缩放、平移）
   - 鼠标交互
   - 颜色模式切换（方向RGB、纯色、随机）
2. 基本功能测试

### 阶段3：性能优化
1. 实现分批渲染
2. 添加LOD系统
3. 视锥体剔除
4. 大数据集测试（400万轨迹）

### 阶段4：集成VTK医学图像功能（可选）
1. 如果需要显示医学图像背景，使用VTKSliceViewer
2. OpenGL纤维束 + VTK切片显示的混合界面
3. 更新用户界面
4. 更新文档

### 阶段5：高级特性（可选）
1. 管状渲染（Tube rendering）
2. 阴影和光照
3. 透明度排序
4. 实时滤波和编辑

## 七、风险与挑战

1. **兼容性** - 需要OpenGL 4.6支持
2. **调试难度** - OpenGL调试比VTK复杂
3. **交互实现** - 需要自己实现鼠标交互

## 八、总结

切换到OpenGL是解决大规模纤维束渲染的最佳方案：
- 完全控制渲染管道
- 避免VTK的内存限制
- 实现DSI Studio级别的性能
- 保留VTK用于医学影像处理的优势