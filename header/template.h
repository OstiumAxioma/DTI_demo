#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <memory>

// 前向声明VTK类，避免在头文件中包含VTK头文件
class vtkRenderer;
class vtkRenderWindow;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkPolyDataMapper;
class vtkSphereSource;

namespace TemplateLib {

    class VTKRenderer {
    public:
        VTKRenderer();
        ~VTKRenderer();

        // 初始化渲染器
        void InitializeRenderer();
        
        // 设置渲染窗口
        void SetRenderWindow(vtkRenderWindow* window);
        
        // 获取渲染器
        vtkRenderer* GetRenderer();
        
        // 创建球体
        void CreateSphere(double radius = 1.0, int phiResolution = 20, int thetaResolution = 20);
        
        // 清除所有Actor
        void ClearActors();
        
        // 重置相机
        void ResetCamera();
        
        // 渲染
        void Render();
        
        // 设置背景颜色
        void SetBackground(double r, double g, double b);

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace TemplateLib

#endif // TEMPLATE_H