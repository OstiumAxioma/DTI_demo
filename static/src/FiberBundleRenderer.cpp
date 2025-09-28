#include "../header/FiberBundleRenderer.h"

#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <algorithm>
#include <random>

namespace DTIFiberLib {

    class FiberBundleRenderer::Impl {
    public:
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> lines;
        vtkSmartPointer<vtkUnsignedCharArray> colors;

        Impl() {
            polyData = vtkSmartPointer<vtkPolyData>::New();
            mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            actor = vtkSmartPointer<vtkActor>::New();
            points = vtkSmartPointer<vtkPoints>::New();
            lines = vtkSmartPointer<vtkCellArray>::New();
            colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        }
    };

    FiberBundleRenderer::FiberBundleRenderer() 
        : m_coloringMode(FiberColoringMode::DIRECTION_RGB)
        , m_lineWidth(1.0f)
        , m_opacity(1.0f)
        , m_renderedTrackCount(0)
        , m_totalPointCount(0)
        , pImpl(std::make_unique<Impl>())
    {
        m_lineColor[0] = 1.0f;
        m_lineColor[1] = 0.0f; 
        m_lineColor[2] = 0.0f;
        
        BuildVTKPipeline();
    }

    FiberBundleRenderer::~FiberBundleRenderer() = default;

    void FiberBundleRenderer::BuildVTKPipeline() {
        pImpl->colors->SetNumberOfComponents(3);
        pImpl->colors->SetName("Colors");

        pImpl->polyData->SetPoints(pImpl->points);
        pImpl->polyData->SetLines(pImpl->lines);
        pImpl->polyData->GetPointData()->SetScalars(pImpl->colors);

        pImpl->mapper->SetInputData(pImpl->polyData);
        pImpl->actor->SetMapper(pImpl->mapper);
        
        pImpl->actor->GetProperty()->SetLineWidth(m_lineWidth);
        pImpl->actor->GetProperty()->SetOpacity(m_opacity);
    }

    void FiberBundleRenderer::SetFiberTracks(const std::vector<FiberTrack>& tracks) {
        m_fiberTracks = tracks;
        UpdateVTKData();
    }

    void FiberBundleRenderer::SetTrackSubset(const std::vector<FiberTrack>& tracks, size_t maxTracks) {
        if (tracks.size() <= maxTracks) {
            m_fiberTracks = tracks;
        } else {
            m_fiberTracks.clear();
            m_fiberTracks.reserve(maxTracks);
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dis(0, static_cast<int>(tracks.size() - 1));
            
            for (size_t i = 0; i < maxTracks; ++i) {
                m_fiberTracks.push_back(tracks[dis(gen)]);
            }
        }
        UpdateVTKData();
    }

    void FiberBundleRenderer::UpdateVTKData() {
        pImpl->points->Reset();
        pImpl->lines->Reset();
        pImpl->colors->Reset();

        m_totalPointCount = 0;
        vtkIdType pointId = 0;

        for (const auto& track : m_fiberTracks) {
            if (track.empty()) continue;

            pImpl->lines->InsertNextCell(static_cast<vtkIdType>(track.size()));
            
            for (const auto& point : track) {
                pImpl->points->InsertNextPoint(point.x, point.y, point.z);
                pImpl->lines->InsertCellPoint(pointId);
                pointId++;
                m_totalPointCount++;
            }
        }

        m_renderedTrackCount = m_fiberTracks.size();

        switch (m_coloringMode) {
            case FiberColoringMode::DIRECTION_RGB:
                SetDirectionColors();
                break;
            case FiberColoringMode::SOLID_COLOR:
                SetSolidColors();
                break;
            case FiberColoringMode::RANDOM_COLORS:
                SetRandomColors();
                break;
        }

        pImpl->polyData->Modified();
    }

    void FiberBundleRenderer::SetDirectionColors() {
        pImpl->colors->SetNumberOfTuples(static_cast<vtkIdType>(m_totalPointCount));
        
        vtkIdType pointIndex = 0;
        for (const auto& track : m_fiberTracks) {
            if (track.size() < 2) {
                if (!track.empty()) {
                    pImpl->colors->SetTuple3(pointIndex++, 128, 128, 128);
                }
                continue;
            }

            for (size_t i = 0; i < track.size(); ++i) {
                float dirX = 0, dirY = 0, dirZ = 0;
                
                if (i == 0) {
                    dirX = track[1].x - track[0].x;
                    dirY = track[1].y - track[0].y;
                    dirZ = track[1].z - track[0].z;
                } else if (i == track.size() - 1) {
                    dirX = track[i].x - track[i-1].x;
                    dirY = track[i].y - track[i-1].y;
                    dirZ = track[i].z - track[i-1].z;
                } else {
                    dirX = track[i+1].x - track[i-1].x;
                    dirY = track[i+1].y - track[i-1].y;
                    dirZ = track[i+1].z - track[i-1].z;
                }

                float length = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
                if (length > 0) {
                    dirX /= length;
                    dirY /= length;
                    dirZ /= length;
                }

                unsigned char r = static_cast<unsigned char>(std::abs(dirX) * 255);
                unsigned char g = static_cast<unsigned char>(std::abs(dirY) * 255);
                unsigned char b = static_cast<unsigned char>(std::abs(dirZ) * 255);

                pImpl->colors->SetTuple3(pointIndex++, r, g, b);
            }
        }
    }

    void FiberBundleRenderer::SetSolidColors() {
        pImpl->colors->SetNumberOfTuples(static_cast<vtkIdType>(m_totalPointCount));
        
        unsigned char r = static_cast<unsigned char>(m_lineColor[0] * 255);
        unsigned char g = static_cast<unsigned char>(m_lineColor[1] * 255);
        unsigned char b = static_cast<unsigned char>(m_lineColor[2] * 255);

        for (vtkIdType i = 0; i < static_cast<vtkIdType>(m_totalPointCount); ++i) {
            pImpl->colors->SetTuple3(i, r, g, b);
        }
    }

    void FiberBundleRenderer::SetRandomColors() {
        pImpl->colors->SetNumberOfTuples(static_cast<vtkIdType>(m_totalPointCount));
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> colorDis(64, 255);

        vtkIdType pointIndex = 0;
        for (const auto& track : m_fiberTracks) {
            unsigned char r = static_cast<unsigned char>(colorDis(gen));
            unsigned char g = static_cast<unsigned char>(colorDis(gen));
            unsigned char b = static_cast<unsigned char>(colorDis(gen));
            
            for (size_t i = 0; i < track.size(); ++i) {
                pImpl->colors->SetTuple3(pointIndex++, r, g, b);
            }
        }
    }

    void FiberBundleRenderer::SetLineColor(float r, float g, float b) {
        m_lineColor[0] = r;
        m_lineColor[1] = g;
        m_lineColor[2] = b;
        
        if (m_coloringMode == FiberColoringMode::SOLID_COLOR) {
            SetSolidColors();
            pImpl->polyData->Modified();
        }
    }

    void FiberBundleRenderer::SetLineWidth(float width) {
        m_lineWidth = width;
        pImpl->actor->GetProperty()->SetLineWidth(width);
    }

    void FiberBundleRenderer::SetOpacity(float opacity) {
        m_opacity = opacity;
        pImpl->actor->GetProperty()->SetOpacity(opacity);
    }

    void FiberBundleRenderer::SetColoringMode(FiberColoringMode mode) {
        if (m_coloringMode != mode) {
            m_coloringMode = mode;
            UpdateVTKData();
        }
    }

    vtkActor* FiberBundleRenderer::GetActor() {
        return pImpl->actor;
    }

    void FiberBundleRenderer::RenderFiberBundle() {
        UpdateVTKData();
    }

    void FiberBundleRenderer::ClearFibers() {
        m_fiberTracks.clear();
        UpdateVTKData();
    }

} // namespace DTIFiberLib