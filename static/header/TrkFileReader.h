#ifndef TRKFILEREADER_H
#define TRKFILEREADER_H

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

namespace DTIFiberLib {

    struct TractographyHeader {
        char magic[6];
        uint16_t dim[3];
        float voxel_size[3];
        float origin[3];
        uint16_t n_scalars;
        char scalar_name[10][20];
        uint16_t n_properties;
        char property_name[10][20];
        float vox_to_ras[4][4];
        char reserved[444];
        char voxel_order[4];
        char pad2[4];
        float image_orientation_patient[6];
        char pad1[2];
        uint8_t invert_x;
        uint8_t invert_y;
        uint8_t invert_z;
        uint8_t swap_xy;
        uint8_t swap_yz;
        uint8_t swap_zx;
        uint32_t n_count;
        uint32_t version;
        uint32_t hdr_size;
    };

    struct TrackPoint {
        float x, y, z;
        std::vector<float> scalars;
    };

    using FiberTrack = std::vector<TrackPoint>;

    class TrkFileReader {
    public:
        TrkFileReader();
        ~TrkFileReader();

        bool LoadTractographyFile(const std::string& filename);
        bool IsValidFile() const { return m_isValidFile; }
        
        const TractographyHeader& GetHeader() const { return m_tractographyHeader; }
        const std::vector<FiberTrack>& GetAllTracks() const { return m_fiberTracks; }
        size_t GetTrackCount() const { return m_fiberTracks.size(); }
        const FiberTrack& GetTrack(size_t index) const;
        
        void PrintHeaderInfo() const;
        const std::string& GetLastErrorMessage() const { return m_lastErrorMessage; }

    private:
        bool ParseTrkHeader();
        bool ExtractFiberTracks();
        bool ValidateFileFormat();
        
        std::ifstream m_file;
        TractographyHeader m_tractographyHeader;
        std::vector<FiberTrack> m_fiberTracks;
        bool m_isValidFile;
        std::string m_lastErrorMessage;
    };

} // namespace DTIFiberLib

#endif // TRKFILEREADER_H