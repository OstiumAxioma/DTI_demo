#ifndef GLFIBERDATA_H
#define GLFIBERDATA_H

#include "TrkFileReader.h"
#include <string>

namespace DTIFiberLib {

/**
 * @brief Container for fiber bundle datasets and visualization parameters.
 *
 * GLFiberData owns copies of the fiber tracks loaded from one or more
 * TrackVis (.trk) files. Each track stores additional metadata so the
 * renderer can control how it should appear on screen.
 */
class GLFiberData {
public:
    enum class TractStyle {
        Line = 0,
        Tube = 1,
        Point = 2
    };

    struct TrackEntry {
        FiberTrack track;                // raw polyline points + optional scalars
        TractStyle style = TractStyle::Line;
        std::vector<float> trackParam;   // user supplied metric (e.g. QA) per point or per track
        std::string datasetName;         // originating .trk file (basename without extension)
        size_t datasetId = 0;            // index into dataset metadata table
    };

    struct DatasetInfo {
        std::string name;
        size_t startIndex = 0;
        size_t trackCount = 0;
        bool visible = true;
    };

    struct BoundingBox {
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
        float minZ = 0.0f;
        float maxZ = 0.0f;
    };

public:
    GLFiberData() = default;

    /**
     * @brief Remove all stored tracks and metadata.
     */
    void clear();

    /**
     * @brief Append a dataset to the container.
     * @param datasetName Basename (without extension) of the originating .trk file.
     * @param tracks Fiber tracks loaded from the file.
     * @param defaultStyle Visualization style assigned to each track.
     * @param perTrackParams Optional metric arrays aligned with @p tracks.
     *        If empty, each track will receive an empty parameter array.
     * @return Number of tracks appended.
     */
    size_t addDataset(const std::string& datasetName,
                      const std::vector<FiberTrack>& tracks,
                      TractStyle defaultStyle = TractStyle::Line,
                      const std::vector<std::vector<float>>& perTrackParams = {});

    /**
     * @return All stored track entries.
     */
    const std::vector<TrackEntry>& getTracks() const { return m_tracks; }

    /**
     * @return Dataset metadata table.
     */
    const std::vector<DatasetInfo>& getDatasets() const { return m_datasets; }

    /**
     * @brief Set visibility for dataset by index.
     */
    bool setDatasetVisibility(size_t datasetIndex, bool visible);

    /**
     * @brief Returns visibility for dataset index.
     */
    bool isDatasetVisible(size_t datasetIndex) const;

    /**
     * @brief Convenience check for a track entry visibility.
     */
    bool isTrackVisible(const TrackEntry& entry) const;

    /**
     * @return True if no track is stored.
     */
    bool empty() const { return m_tracks.empty(); }

    /**
     * @return Aggregate bounding box covering every stored track.
     */
    BoundingBox computeBoundingBox() const;

private:
    std::vector<TrackEntry> m_tracks;
    std::vector<DatasetInfo> m_datasets;
};

} // namespace DTIFiberLib

#endif // GLFIBERDATA_H
