#ifndef FIBERBUNDLERENDERER_H
#define FIBERBUNDLERENDERER_H

#include "TrkFileReader.h"
#include <memory>

// VTK前向声明
class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkPoints;
class vtkCellArray;
class vtkUnsignedCharArray;

namespace DTIFiberLib {

    enum class FiberColoringMode {
        SOLID_COLOR,      
        DIRECTION_RGB,    
        RANDOM_COLORS     
    };

    class FiberBundleRenderer {
    public:
        FiberBundleRenderer();
        ~FiberBundleRenderer();

        void SetFiberTracks(const std::vector<FiberTrack>& tracks);
        void SetTrackSubset(const std::vector<FiberTrack>& tracks, size_t maxTracks = 1000);
        
        void SetLineColor(float r, float g, float b);
        void SetLineWidth(float width);
        void SetOpacity(float opacity);
        void SetColoringMode(FiberColoringMode mode);

        vtkActor* GetActor();
        void RenderFiberBundle();
        
        size_t GetRenderedTrackCount() const { return m_renderedTrackCount; }
        size_t GetTotalPointCount() const { return m_totalPointCount; }

        void ClearFibers();

    private:
        void BuildVTKPipeline();
        void SetDirectionColors();
        void SetSolidColors();
        void SetRandomColors();
        void UpdateVTKData();

        std::vector<FiberTrack> m_fiberTracks;
        FiberColoringMode m_coloringMode;
        float m_lineColor[3];
        float m_lineWidth;
        float m_opacity;
        
        size_t m_renderedTrackCount;
        size_t m_totalPointCount;

        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace DTIFiberLib

#endif // FIBERBUNDLERENDERER_H