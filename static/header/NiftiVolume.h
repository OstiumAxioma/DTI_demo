#ifndef NIFTI_VOLUME_H
#define NIFTI_VOLUME_H

#include "GLFiberData.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>

namespace DTIFiberLib {

enum class SliceAxis {
    Sagittal = 0, // X axis (I)
    Coronal = 1,  // Y axis (J)
    Axial = 2     // Z axis (K)
};

/**
 * Lightweight container around a NIfTI volume loaded via VTK.
 * Provides access to voxel-space metadata and helpers to extract
 * normalized 8-bit slices for GPU upload.
 */
class NiftiVolume {
public:
    NiftiVolume();
    ~NiftiVolume();

    bool loadFromFile(const std::string& filePath);
    bool isLoaded() const { return m_isLoaded; }

    const std::string& getFilePath() const { return m_filePath; }
    const std::array<int, 3>& getDimensions() const { return m_dimensions; }
    const std::array<double, 3>& getSpacing() const { return m_spacing; }
    const std::array<double, 3>& getOrigin() const { return m_origin; }
    const std::array<double, 2>& getScalarRange() const { return m_scalarRange; }

    int getSliceCount(SliceAxis axis) const;
    int getExtentMin(SliceAxis axis) const;
    int getExtentMax(SliceAxis axis) const;

    // Slider value (0..sliceCount-1) to voxel index (extent-based)
    int sliderToVoxel(SliceAxis axis, int sliderValue) const;

    // Extract a grayscale slice normalized to [0, 255]
    bool extractSlice(SliceAxis axis,
                      int voxelIndex,
                      std::vector<uint8_t>& outPixels,
                      int& width,
                      int& height) const;

    // Convert voxel coordinate to world (RAS) using stored transform.
    std::array<double, 3> voxelToWorld(double i, double j, double k) const;

    // Compute world-space bounding box of the volume.
    GLFiberData::BoundingBox computeBoundingBox() const;

private:
    vtkSmartPointer<vtkImageData> m_imageData;
    vtkSmartPointer<vtkMatrix4x4> m_ijkToRas;

    std::array<int, 3> m_dimensions;
    int m_extent[6];
    std::array<double, 3> m_spacing;
    std::array<double, 3> m_origin;
    std::array<double, 2> m_scalarRange;
    std::string m_filePath;

    int m_componentCount;
    int m_scalarType;
    bool m_isLoaded;

    double normalizeValue(double value) const;
};

} // namespace DTIFiberLib

#endif // NIFTI_VOLUME_H
