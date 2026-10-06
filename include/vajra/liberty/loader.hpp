#pragma once

#include "vajra/liberty/models.hpp"
#include <map>
#include <string>
#include <utility>

namespace vajra::liberty {

class Loader {
public:
    // Transparently loads an uncompressed .lib or gzip-compressed .lib.gz file into memory
    static std::string load_file(const std::string& filepath);

    // Checks whether a file starts with gzip magic header (0x1F, 0x8B)
    static bool is_gzip(const std::string& filepath);
};

// Builds scan replacement mapping pairing regular flip-flops to scan equivalents
std::pair<std::map<std::string, std::string>, std::map<std::string, ScanReplacementInfo>>
build_scan_replacement_map(const Library& library);

} // namespace vajra::liberty
