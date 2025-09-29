#include "../header/DTIRenderer.h"

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>

namespace DTIFiberLib {

    // 使用Pimpl模式隐藏VTK实现细节
    class DTIFiberRenderer::Impl {
    public:
        vtkSmartPointer<vtkRenderer> renderer;
        vtkSmartPointer<vtkRenderWindow> renderWindow;
        vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;
        
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