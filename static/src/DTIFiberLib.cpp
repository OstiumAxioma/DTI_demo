#include "DTIFiberLib.h"

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkSphereSource.h>
#include <vtkCamera.h>

namespace DTIFiberLib {

    // 使用Pimpl模式隐藏VTK实现细节
    class DTIFiberRenderer::Impl {
    public:
        vtkSmartPointer<vtkRenderer> renderer;
        vtkSmartPointer<vtkRenderWindow> renderWindow;
        vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkSphereSource> sphereSource;
        
        Impl() {
            renderer = vtkSmartPointer<vtkRenderer>::New();
        }
    };

    DTIFiberRenderer::DTIFiberRenderer() : pImpl(std::make_unique<Impl>()) {
    }

    DTIFiberRenderer::~DTIFiberRenderer() = default;

    void DTIFiberRenderer::InitializeRenderer() {
        if (pImpl->renderer) {
            pImpl->renderer->SetBackground(0.1, 0.2, 0.4); // 默认深蓝色背景
        }
    }

    void DTIFiberRenderer::SetRenderWindow(vtkRenderWindow* window) {
        if (window && pImpl->renderer) {
            pImpl->renderWindow = window;
            pImpl->renderWindow->AddRenderer(pImpl->renderer);
            
            // 设置交互器
            pImpl->renderWindowInteractor = pImpl->renderWindow->GetInteractor();
            if (pImpl->renderWindowInteractor) {
                pImpl->renderWindowInteractor->SetRenderWindow(pImpl->renderWindow);
                pImpl->renderWindowInteractor->Initialize();
            }
        }
    }

    vtkRenderer* DTIFiberRenderer::GetRenderer() {
        return pImpl->renderer;
    }

    void DTIFiberRenderer::CreateSphere(double radius, int phiResolution, int thetaResolution) {
        // 创建球体源
        pImpl->sphereSource = vtkSmartPointer<vtkSphereSource>::New();
        pImpl->sphereSource->SetRadius(radius);
        pImpl->sphereSource->SetPhiResolution(phiResolution);
        pImpl->sphereSource->SetThetaResolution(thetaResolution);

        // 创建映射器
        pImpl->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        pImpl->mapper->SetInputConnection(pImpl->sphereSource->GetOutputPort());

        // 创建演员
        pImpl->actor = vtkSmartPointer<vtkActor>::New();
        pImpl->actor->SetMapper(pImpl->mapper);

        // 添加演员到渲染器
        if (pImpl->renderer) {
            pImpl->renderer->AddActor(pImpl->actor);
        }
    }

    void DTIFiberRenderer::ClearActors() {
        if (pImpl->renderer) {
            pImpl->renderer->RemoveAllViewProps();  // VTK 8.2使用RemoveAllViewProps
        }
    }

    void DTIFiberRenderer::ResetCamera() {
        if (pImpl->renderer) {
            pImpl->renderer->ResetCamera();
        }
    }

    void DTIFiberRenderer::Render() {
        if (pImpl->renderWindow) {
            pImpl->renderWindow->Render();
        }
    }

    void DTIFiberRenderer::SetBackground(double r, double g, double b) {
        if (pImpl->renderer) {
            pImpl->renderer->SetBackground(r, g, b);
        }
    }

} // namespace DTIFiberLib