# DTIFiberLib 静态库

DTI Fiber Viewer项目的核心静态库，专门用于处理DTI神经纤维束数据的解析、处理和渲染。

## 库目标

本静态库的主要目标是提供一套完整的DTI纤维束数据处理解决方案：

- **文件解析**：支持.trk等神经纤维束文件格式的读取和解析
- **数据结构**：提供高效的纤维束数据表示和存储结构
- **渲染引擎**：基于VTK的高性能3D纤维束渲染算法
- **处理算法**：纤维束过滤、聚类、统计分析等核心算法
- **可视化控制**：颜色映射、透明度、LOD等可视化参数控制

## 架构设计

```
static/
├── header/                 # 公共头文件
│   └── DTIFiberLib.h      # 主要API接口
├── src/                   # 源代码实现
│   └── DTIFiberLib.cpp    # 主要实现文件
├── build/                 # 构建输出目录
├── build.bat             # 构建脚本
└── CMakeLists.txt        # CMake配置文件
```

## 环境要求
- Visual Studio 2022
- CMake 3.20+
- VTK 8.2.0
- Qt 5.14.2（VTK需要）

## 配置方法

### 方法1：使用config.local.cmake（推荐）
1. 复制 `config.cmake` 为 `config.local.cmake`
2. 编辑 `config.local.cmake`，取消注释并设置正确的路径：
   ```cmake
   set(Qt5_DIR "C:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5")
   set(VTK_DIR "D:/code/vtk8.2.0/VTK-8.2.0/lib/cmake/vtk-8.2")
   ```
3. 运行 `build.bat`

### 方法2：使用命令行参数
直接运行build.bat时传入路径：
```batch
build.bat "C:/Qt/Qt5.14.2/5.14.2/msvc2017_64" "D:/code/vtk8.2.0/VTK-8.2.0/lib/cmake/vtk-8.2"
```

### 方法3：设置环境变量
在系统环境变量中设置：
- `Qt5_DIR` = `C:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5`
- `VTK_DIR` = `D:/code/vtk8.2.0/VTK-8.2.0/lib/cmake/vtk-8.2`

### 方法4：自动检测
如果Qt和VTK安装在常见路径，CMake会尝试自动检测。支持的路径包括：
- Qt: `C:/Qt/Qt5.14.2/`, `C:/Qt/5.14.2/`, `D:/Qt/Qt5.14.2/` 等
- VTK: `D:/code/vtk8.2.0/VTK-8.2.0/`, `C:/VTK/`, `C:/Program Files/VTK/` 等

## 编译选项
运行 `build.bat` 后会提示选择编译配置：
1. Debug - 调试版本
2. Release - 发布版本
3. Both - 同时编译两个版本（默认）

## 输出文件
编译成功后：
- 静态库文件：`../lib/DTIFiberLib.lib`（Release）和 `../lib/DTIFiberLib_d.lib`（Debug）
- 头文件：`../header/DTIFiberLib.h`

## 核心模块

### 当前实现

#### DTIFiberRenderer类
- **功能**：基础VTK渲染器封装
- **特点**：使用Pimpl模式隐藏VTK实现细节
- **方法**：
  - `InitializeRenderer()` - 初始化渲染器
  - `SetRenderWindow()` - 设置渲染窗口
  - `CreateSphere()` - 创建测试球体
  - `SetBackground()` - 设置背景颜色
  - `Render()` - 执行渲染

### 计划实现

#### TrkFileReader类
- **功能**：TrackVis (.trk) 文件格式解析器
- **特点**：支持TrackVis文件头解析和纤维束数据读取

#### FiberTrack类
- **功能**：单条纤维束轨迹数据结构
- **特点**：存储3D空间中的连续点序列

#### FiberBundle类
- **功能**：纤维束集合管理器
- **特点**：管理多条纤维束轨迹，提供批量操作

#### FiberRenderer类
- **功能**：专业的纤维束渲染器
- **特点**：高效的纤维束可视化，支持多种渲染模式

## 开发内容规划

### 阶段1：数据结构设计 🔄
- [ ] 定义FiberTrack数据结构
- [ ] 实现FiberBundle容器类
- [ ] 设计内存管理策略

### 阶段2：文件格式支持
- [ ] .trk文件格式解析
- [ ] 文件头信息提取
- [ ] 错误处理和验证

### 阶段3：渲染算法优化
- [ ] 高效的线条渲染
- [ ] LOD (Level of Detail) 支持
- [ ] GPU加速渲染

### 阶段4：高级功能
- [ ] 纤维束聚类算法
- [ ] 统计分析功能
- [ ] 多种颜色映射模式

## 使用示例

```cpp
#include "DTIFiberLib.h"
using namespace DTIFiberLib;

// 创建渲染器
auto renderer = std::make_unique<DTIFiberRenderer>();
renderer->InitializeRenderer();

// 加载纤维束数据 (计划功能)
// TrkFileReader reader;
// auto fiberBundle = reader.LoadTrkFile("fibers.trk");

// 渲染纤维束 (计划功能)
// renderer->RenderFibers(fiberBundle.get());
```

## 故障排除
1. **找不到Qt5**：确保Qt5路径正确，并包含msvc2017_64编译器版本
2. **找不到VTK**：确保VTK已正确编译并安装
3. **编译失败**：检查Visual Studio 2022是否正确安装

## 在其他项目中使用
1. 将 `lib` 文件夹中的静态库添加到你的项目
2. 包含 `header` 文件夹中的头文件
3. 确保链接了VTK和Qt5的依赖库

---

*最后更新：2025年9月26日*