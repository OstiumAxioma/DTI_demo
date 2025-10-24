#include "../header/GLFiberData.h"
#include <algorithm>
#include <limits>

namespace DTIFiberLib {

void GLFiberData::clear()
{
    m_tracks.clear();
}

size_t GLFiberData::addDataset(const std::string& datasetName,
                               const std::vector<FiberTrack>& tracks,
                               TractStyle defaultStyle,
                               const std::vector<std::vector<float>>& perTrackParams)
{
    const size_t originalSize = m_tracks.size();
    const bool hasParams = !perTrackParams.empty();

    if (hasParams && perTrackParams.size() != tracks.size()) {
        // Parameter count mismatch – ignore custom params for safety.
        // This mirrors defensive checks in UI code that may build the arrays.
    }

    for (size_t i = 0; i < tracks.size(); ++i) {
        TrackEntry entry;
        entry.track = tracks[i];
        entry.style = defaultStyle;
        entry.datasetName = datasetName;

        if (hasParams && i < perTrackParams.size()) {
            entry.trackParam = perTrackParams[i];
        }

        m_tracks.push_back(std::move(entry));
    }

    return m_tracks.size() - originalSize;
}

GLFiberData::BoundingBox GLFiberData::computeBoundingBox() const
{
    BoundingBox box{};
    if (m_tracks.empty()) {
        return box;
    }

    box.minX = box.minY = box.minZ = std::numeric_limits<float>::max();
    box.maxX = box.maxY = box.maxZ = std::numeric_limits<float>::lowest();

    for (const auto& entry : m_tracks) {
        for (const auto& point : entry.track) {
            box.minX = std::min(box.minX, point.x);
            box.minY = std::min(box.minY, point.y);
            box.minZ = std::min(box.minZ, point.z);

            box.maxX = std::max(box.maxX, point.x);
            box.maxY = std::max(box.maxY, point.y);
            box.maxZ = std::max(box.maxZ, point.z);
        }
    }

    return box;
}

} // namespace DTIFiberLib
