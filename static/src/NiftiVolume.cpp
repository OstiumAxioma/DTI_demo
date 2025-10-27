#include "../header/NiftiVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNIFTIImageReader.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>

namespace DTIFiberLib {
namespace {

template <typename T>
inline double readComponent(void* ptr, int componentCount)
{
    auto typed = static_cast<T*>(ptr);
    return componentCount > 0 ? static_cast<double>(typed[0]) : 0.0;
}

inline double clamp01(double value)
{
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

constexpr std::array<std::array<int, 2>, 3> kPlaneAxes = {
    std::array<int, 2>{1, 2}, // Sagittal -> vary J, K
    std::array<int, 2>{0, 2}, // Coronal  -> vary I, K
    std::array<int, 2>{0, 1}  // Axial    -> vary I, J
};

} // namespace

NiftiVolume::NiftiVolume()
    : m_dimensions{0, 0, 0}
    , m_spacing{1.0, 1.0, 1.0}
    , m_origin{0.0, 0.0, 0.0}
    , m_scalarRange{0.0, 1.0}
    , m_componentCount(1)
    , m_scalarType(VTK_VOID)
    , m_isLoaded(false)
{
    std::fill(std::begin(m_extent), std::end(m_extent), 0);
}

NiftiVolume::~NiftiVolume() = default;

bool NiftiVolume::loadFromFile(const std::string& filePath)
{
    vtkNew<vtkNIFTIImageReader> reader;
    reader->SetFileName(filePath.c_str());
    reader->Update();

    vtkImageData* rawImage = reader->GetOutput();
    if (!rawImage) {
        return false;
    }

    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->DeepCopy(rawImage);
    m_imageData = image;

    int dims[3] = {0};
    image->GetDimensions(dims);
    for (int i = 0; i < 3; ++i) {
        m_dimensions[i] = dims[i];
    }

    image->GetExtent(m_extent);
    image->GetSpacing(m_spacing.data());
    image->GetOrigin(m_origin.data());

    double range[2] = {0.0, 1.0};
    image->GetScalarRange(range);
    if (range[1] <= range[0]) {
        range[1] = range[0] + 1.0;
    }
    m_scalarRange = {range[0], range[1]};

    m_componentCount = std::max(1, image->GetNumberOfScalarComponents());
    m_scalarType = image->GetScalarType();

    vtkSmartPointer<vtkMatrix4x4> transform = vtkSmartPointer<vtkMatrix4x4>::New();
    bool copiedMatrix = false;
    if (reader->GetQFormMatrix()) {
        transform->DeepCopy(reader->GetQFormMatrix());
        copiedMatrix = true;
    } else if (reader->GetSFormMatrix()) {
        transform->DeepCopy(reader->GetSFormMatrix());
        copiedMatrix = true;
    }

    if (copiedMatrix) {
        for (int axis = 0; axis < 3; ++axis) {
            double colNorm = std::sqrt(
                transform->GetElement(0, axis) * transform->GetElement(0, axis) +
                transform->GetElement(1, axis) * transform->GetElement(1, axis) +
                transform->GetElement(2, axis) * transform->GetElement(2, axis));

            const double desired = std::abs(m_spacing[axis]);
            if (desired <= 1e-6) {
                continue;
            }

            if (std::abs(colNorm - desired) > 1e-4) {
                const double scale = desired / (colNorm > 1e-6 ? colNorm : 1.0);
                for (int row = 0; row < 3; ++row) {
                    transform->SetElement(row, axis, transform->GetElement(row, axis) * scale);
                }
            }
        }
    } else {
        transform->Identity();
        for (int i = 0; i < 3; ++i) {
            transform->SetElement(i, i, m_spacing[i]);
            transform->SetElement(i, 3, m_origin[i]);
        }
    }

    m_ijkToRas = transform;
    m_filePath = filePath;
    m_isLoaded = true;

    std::cout << "Loaded NIfTI volume: " << filePath << std::endl;
    std::cout << "  Dimensions: " << dims[0] << " x " << dims[1] << " x " << dims[2] << std::endl;
    std::cout << "  Spacing: " << m_spacing[0] << ", " << m_spacing[1] << ", " << m_spacing[2] << std::endl;
    std::cout << "  Origin: " << m_origin[0] << ", " << m_origin[1] << ", " << m_origin[2] << std::endl;
    if (m_ijkToRas) {
        std::cout << "  IJK->RAS matrix:" << std::endl;
        for (int r = 0; r < 4; ++r) {
            std::cout << "    ";
            for (int c = 0; c < 4; ++c) {
                std::cout << m_ijkToRas->GetElement(r, c) << (c < 3 ? ", " : "");
            }
            std::cout << std::endl;
        }
    }

    return true;
}

int NiftiVolume::getSliceCount(SliceAxis axis) const
{
    if (!m_isLoaded) {
        return 0;
    }
    const int idx = static_cast<int>(axis);
    return m_extent[idx * 2 + 1] - m_extent[idx * 2] + 1;
}

int NiftiVolume::getExtentMin(SliceAxis axis) const
{
    const int idx = static_cast<int>(axis);
    return m_extent[idx * 2];
}

int NiftiVolume::getExtentMax(SliceAxis axis) const
{
    const int idx = static_cast<int>(axis);
    return m_extent[idx * 2 + 1];
}

int NiftiVolume::sliderToVoxel(SliceAxis axis, int sliderValue) const
{
    return getExtentMin(axis) + sliderValue;
}

double NiftiVolume::normalizeValue(double value) const
{
    const double denom = std::max(m_scalarRange[1] - m_scalarRange[0], 1e-8);
    return clamp01((value - m_scalarRange[0]) / denom);
}

bool NiftiVolume::extractSlice(SliceAxis axis,
                               int voxelIndex,
                               std::vector<uint8_t>& outPixels,
                               int& width,
                               int& height) const
{
    if (!m_isLoaded || !m_imageData) {
        return false;
    }

    const int axisIdx = static_cast<int>(axis);
    const int minIdx = getExtentMin(axis);
    const int maxIdx = getExtentMax(axis);
    if (voxelIndex < minIdx || voxelIndex > maxIdx) {
        return false;
    }

    const int axisA = kPlaneAxes[axisIdx][0];
    const int axisB = kPlaneAxes[axisIdx][1];
    const int countA = m_extent[axisA * 2 + 1] - m_extent[axisA * 2] + 1;
    const int countB = m_extent[axisB * 2 + 1] - m_extent[axisB * 2] + 1;
    width = countA;
    height = countB;

    outPixels.assign(static_cast<size_t>(width) * height, 0);

    for (int b = 0; b < countB; ++b) {
        const int voxelB = m_extent[axisB * 2] + b;
        for (int a = 0; a < countA; ++a) {
            const int voxelA = m_extent[axisA * 2] + a;

            int ijk[3];
            ijk[axisIdx] = voxelIndex;
            ijk[axisA] = voxelA;
            ijk[axisB] = voxelB;

            void* ptr = m_imageData->GetScalarPointer(ijk[0], ijk[1], ijk[2]);
            double value = 0.0;
            if (!ptr) {
                value = m_scalarRange[0];
            } else {

                switch (m_scalarType) {
                case VTK_UNSIGNED_CHAR: value = readComponent<unsigned char>(ptr, m_componentCount); break;
                case VTK_CHAR:
                case VTK_SIGNED_CHAR: value = readComponent<signed char>(ptr, m_componentCount); break;
                case VTK_SHORT: value = readComponent<short>(ptr, m_componentCount); break;
                case VTK_UNSIGNED_SHORT: value = readComponent<unsigned short>(ptr, m_componentCount); break;
                case VTK_INT: value = readComponent<int>(ptr, m_componentCount); break;
                case VTK_UNSIGNED_INT: value = readComponent<unsigned int>(ptr, m_componentCount); break;
                case VTK_FLOAT: value = readComponent<float>(ptr, m_componentCount); break;
                case VTK_DOUBLE: value = readComponent<double>(ptr, m_componentCount); break;
                case VTK_LONG_LONG: value = readComponent<long long>(ptr, m_componentCount); break;
                case VTK_UNSIGNED_LONG_LONG: value = readComponent<unsigned long long>(ptr, m_componentCount); break;
                default: value = 0.0; break;
                }
            }

            const double normalized = normalizeValue(value);
            const int pixelIndex = b * width + a;
            outPixels[pixelIndex] = static_cast<uint8_t>(std::lround(normalized * 255.0));
        }
    }

    return true;
}

std::array<double, 3> NiftiVolume::voxelToWorld(double i, double j, double k) const
{
    std::array<double, 3> world{0.0, 0.0, 0.0};
    if (!m_ijkToRas) {
        world[0] = m_origin[0] + i * m_spacing[0];
        world[1] = m_origin[1] + j * m_spacing[1];
        world[2] = m_origin[2] + k * m_spacing[2];
        return world;
    }

    double in[4] = {i, j, k, 1.0};
    double out[4] = {0.0, 0.0, 0.0, 1.0};
    m_ijkToRas->MultiplyPoint(in, out);
    world[0] = out[0];
    world[1] = out[1];
    world[2] = out[2];
    return world;
}

GLFiberData::BoundingBox NiftiVolume::computeBoundingBox() const
{
    GLFiberData::BoundingBox box{};
    if (!m_isLoaded) {
        return box;
    }

    const double iEdges[2] = {
        static_cast<double>(m_extent[0]) - 0.5,
        static_cast<double>(m_extent[1]) + 0.5
    };
    const double jEdges[2] = {
        static_cast<double>(m_extent[2]) - 0.5,
        static_cast<double>(m_extent[3]) + 0.5
    };
    const double kEdges[2] = {
        static_cast<double>(m_extent[4]) - 0.5,
        static_cast<double>(m_extent[5]) + 0.5
    };

    double minVals[3] = {std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max()};
    double maxVals[3] = {std::numeric_limits<double>::lowest(),
                         std::numeric_limits<double>::lowest(),
                         std::numeric_limits<double>::lowest()};

    for (double i : iEdges) {
        for (double j : jEdges) {
            for (double k : kEdges) {
                auto world = voxelToWorld(i, j, k);
                for (int axis = 0; axis < 3; ++axis) {
                    minVals[axis] = std::min(minVals[axis], world[axis]);
                    maxVals[axis] = std::max(maxVals[axis], world[axis]);
                }
            }
        }
    }

    box.minX = static_cast<float>(minVals[0]);
    box.maxX = static_cast<float>(maxVals[0]);
    box.minY = static_cast<float>(minVals[1]);
    box.maxY = static_cast<float>(maxVals[1]);
    box.minZ = static_cast<float>(minVals[2]);
    box.maxZ = static_cast<float>(maxVals[2]);
    return box;
}

} // namespace DTIFiberLib
