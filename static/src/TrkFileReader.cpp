#include "../header/TrkFileReader.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>

namespace DTIFiberLib {

    TrkFileReader::TrkFileReader() : m_isValidFile(false) {
        std::memset(&m_tractographyHeader, 0, sizeof(TractographyHeader));
    }

    TrkFileReader::~TrkFileReader() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    bool TrkFileReader::LoadTractographyFile(const std::string& filename) {
        m_isValidFile = false;
        m_fiberTracks.clear();
        m_lastErrorMessage.clear();

        m_file.open(filename, std::ios::binary);
        if (!m_file.is_open()) {
            m_lastErrorMessage = "Cannot open file: " + filename;
            return false;
        }

        if (!ParseTrkHeader()) {
            m_file.close();
            return false;
        }

        if (!ExtractFiberTracks()) {
            m_file.close();
            return false;
        }

        m_file.close();
        m_isValidFile = true;
        m_lastErrorMessage = "Successfully loaded " + std::to_string(m_fiberTracks.size()) + " fiber tracks";
        return true;
    }

    bool TrkFileReader::ParseTrkHeader() {
        m_file.read(m_tractographyHeader.magic, 6);
        if (std::strncmp(m_tractographyHeader.magic, "TRACK", 5) != 0) {
            m_lastErrorMessage = "Invalid file format: not a valid TRK file";
            return false;
        }

        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.dim), sizeof(uint16_t) * 3);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.voxel_size), sizeof(float) * 3);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.origin), sizeof(float) * 3);
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.n_scalars), sizeof(uint16_t));

        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.scalar_name), sizeof(char) * 10 * 20);
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.n_properties), sizeof(uint16_t));
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.property_name), sizeof(char) * 10 * 20);

        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.vox_to_ras), sizeof(float) * 16);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.reserved), sizeof(char) * 444);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.voxel_order), sizeof(char) * 4);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.pad2), sizeof(char) * 4);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.image_orientation_patient), sizeof(float) * 6);
        m_file.read(reinterpret_cast<char*>(m_tractographyHeader.pad1), sizeof(char) * 2);

        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.invert_x), sizeof(uint8_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.invert_y), sizeof(uint8_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.invert_z), sizeof(uint8_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.swap_xy), sizeof(uint8_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.swap_yz), sizeof(uint8_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.swap_zx), sizeof(uint8_t));

        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.n_count), sizeof(uint32_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.version), sizeof(uint32_t));
        m_file.read(reinterpret_cast<char*>(&m_tractographyHeader.hdr_size), sizeof(uint32_t));

        if (m_tractographyHeader.hdr_size != 1000) {
            m_lastErrorMessage = "Invalid header size, may need byte order conversion";
        }

        return ValidateFileFormat();
    }

    bool TrkFileReader::ExtractFiberTracks() {
        m_file.seekg(1000, std::ios::beg);
        m_fiberTracks.clear();

        size_t trackIndex = 0;
        while (!m_file.eof()) {
            uint32_t n_points;
            m_file.read(reinterpret_cast<char*>(&n_points), sizeof(uint32_t));

            if (m_file.eof()) {
                break;
            }

            // Debug: Print first few tracks
            if (trackIndex < 5) {
                std::cout << "Track " << trackIndex << ": n_points = " << n_points << std::endl;
            }

            if (n_points == 0 || n_points > 10000) {
                std::cerr << "WARNING: Track " << trackIndex << " has invalid point count: " << n_points << std::endl;
                break;
            }

            trackIndex++;

            FiberTrack track;
            track.reserve(n_points);

            // Debug: Record file position before reading points
            std::streampos posBefore = m_file.tellg();

            for (uint32_t i = 0; i < n_points; ++i) {
                TrackPoint point;

                m_file.read(reinterpret_cast<char*>(&point.x), sizeof(float));
                m_file.read(reinterpret_cast<char*>(&point.y), sizeof(float));
                m_file.read(reinterpret_cast<char*>(&point.z), sizeof(float));

                if (m_tractographyHeader.n_scalars > 0) {
                    point.scalars.resize(m_tractographyHeader.n_scalars);
                    m_file.read(reinterpret_cast<char*>(point.scalars.data()),
                               sizeof(float) * m_tractographyHeader.n_scalars);
                }

                track.push_back(point);
            }

            if (m_tractographyHeader.n_properties > 0) {
                m_file.seekg(sizeof(float) * m_tractographyHeader.n_properties, std::ios::cur);
            }

            // Debug: Print actual bytes read for first track
            if (trackIndex == 1) {
                std::streampos posAfter = m_file.tellg();
                std::cout << "Track 0: Expected bytes = " << (4 + n_points * 12)
                          << ", Actual bytes read = " << (posAfter - posBefore) << std::endl;
            }

            m_fiberTracks.push_back(track);
        }

        return true;
    }

    bool TrkFileReader::ValidateFileFormat() {
        if (m_tractographyHeader.dim[0] == 0 || m_tractographyHeader.dim[1] == 0 || m_tractographyHeader.dim[2] == 0) {
            m_lastErrorMessage = "Invalid volume dimensions";
            return false;
        }

        if (m_tractographyHeader.voxel_size[0] <= 0 || m_tractographyHeader.voxel_size[1] <= 0 || m_tractographyHeader.voxel_size[2] <= 0) {
            m_lastErrorMessage = "Invalid voxel size";
            return false;
        }

        return true;
    }

    const FiberTrack& TrkFileReader::GetTrack(size_t index) const {
        if (index >= m_fiberTracks.size()) {
            throw std::out_of_range("Track index out of range");
        }
        return m_fiberTracks[index];
    }

    void TrkFileReader::PrintHeaderInfo() const {
        std::cout << "=== TRK File Header Information ===" << std::endl;
        std::cout << "Magic string: " << std::string(m_tractographyHeader.magic, 5) << std::endl;
        std::cout << "Dimensions: " << m_tractographyHeader.dim[0] << " x " 
                  << m_tractographyHeader.dim[1] << " x " << m_tractographyHeader.dim[2] << std::endl;
        std::cout << "Voxel size: " << m_tractographyHeader.voxel_size[0] << " x " 
                  << m_tractographyHeader.voxel_size[1] << " x " << m_tractographyHeader.voxel_size[2] << std::endl;
        std::cout << "Track count (header): " << m_tractographyHeader.n_count << std::endl;
        std::cout << "Version: " << m_tractographyHeader.version << std::endl;
        std::cout << "Scalar count: " << m_tractographyHeader.n_scalars << std::endl;
        std::cout << "Property count: " << m_tractographyHeader.n_properties << std::endl;
        std::cout << "Header size: " << m_tractographyHeader.hdr_size << std::endl;
        std::cout << "Actual loaded tracks: " << m_fiberTracks.size() << std::endl;
    }

    bool TrkFileReader::ExportToJSON(const std::string& outputPath, size_t maxTracks) const {
        if (!m_isValidFile || m_fiberTracks.empty()) {
            return false;
        }

        std::ofstream jsonFile(outputPath);
        if (!jsonFile.is_open()) {
            return false;
        }

        jsonFile << "{\n";
        
        // 输出头部信息
        jsonFile << "  \"header\": {\n";
        jsonFile << "    \"magic\": \"" << std::string(m_tractographyHeader.magic, 5) << "\",\n";
        jsonFile << "    \"dimensions\": [" << m_tractographyHeader.dim[0] << ", " 
                 << m_tractographyHeader.dim[1] << ", " << m_tractographyHeader.dim[2] << "],\n";
        jsonFile << "    \"voxel_size\": [" << m_tractographyHeader.voxel_size[0] << ", " 
                 << m_tractographyHeader.voxel_size[1] << ", " << m_tractographyHeader.voxel_size[2] << "],\n";
        jsonFile << "    \"origin\": [" << m_tractographyHeader.origin[0] << ", " 
                 << m_tractographyHeader.origin[1] << ", " << m_tractographyHeader.origin[2] << "],\n";
        jsonFile << "    \"track_count\": " << m_tractographyHeader.n_count << ",\n";
        jsonFile << "    \"version\": " << m_tractographyHeader.version << ",\n";
        jsonFile << "    \"n_scalars\": " << m_tractographyHeader.n_scalars << ",\n";
        jsonFile << "    \"n_properties\": " << m_tractographyHeader.n_properties << "\n";
        jsonFile << "  },\n";

        // 输出轨迹数据
        size_t tracksToExport = std::min(maxTracks, m_fiberTracks.size());
        jsonFile << "  \"tracks\": [\n";
        
        for (size_t trackIdx = 0; trackIdx < tracksToExport; ++trackIdx) {
            const auto& track = m_fiberTracks[trackIdx];
            jsonFile << "    {\n";
            jsonFile << "      \"track_id\": " << trackIdx << ",\n";
            jsonFile << "      \"point_count\": " << track.size() << ",\n";
            jsonFile << "      \"points\": [\n";
            
            for (size_t pointIdx = 0; pointIdx < track.size(); ++pointIdx) {
                const auto& point = track[pointIdx];
                jsonFile << "        {\"x\": " << point.x << ", \"y\": " << point.y << ", \"z\": " << point.z;
                
                if (!point.scalars.empty()) {
                    jsonFile << ", \"scalars\": [";
                    for (size_t i = 0; i < point.scalars.size(); ++i) {
                        if (i > 0) jsonFile << ", ";
                        jsonFile << point.scalars[i];
                    }
                    jsonFile << "]";
                }
                
                jsonFile << "}";
                if (pointIdx < track.size() - 1) jsonFile << ",";
                jsonFile << "\n";
            }
            
            jsonFile << "      ]\n";
            jsonFile << "    }";
            if (trackIdx < tracksToExport - 1) jsonFile << ",";
            jsonFile << "\n";
        }
        
        jsonFile << "  ],\n";
        jsonFile << "  \"exported_count\": " << tracksToExport << ",\n";
        jsonFile << "  \"total_tracks\": " << m_fiberTracks.size() << "\n";
        jsonFile << "}\n";

        jsonFile.close();
        return true;
    }

} // namespace DTIFiberLib