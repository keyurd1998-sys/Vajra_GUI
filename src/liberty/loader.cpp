#include "vajra/liberty/loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <zlib.h>

namespace vajra::liberty {

bool Loader::is_gzip(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    unsigned char header[2] = {0, 0};
    file.read(reinterpret_cast<char*>(header), 2);
    if (file.gcount() < 2) {
        return false;
    }
    return (header[0] == 0x1F && header[1] == 0x8B);
}

std::string Loader::load_file(const std::string& filepath) {
    if (is_gzip(filepath)) {
        gzFile gz = gzopen(filepath.c_str(), "rb");
        if (!gz) {
            throw std::runtime_error("Failed to open gzip file: " + filepath);
        }

        std::string buffer;
        constexpr size_t CHUNK_SIZE = 65536; // 64 KB chunks
        std::vector<char> chunk(CHUNK_SIZE);

        while (true) {
            int bytes_read = gzread(gz, chunk.data(), static_cast<unsigned int>(CHUNK_SIZE));
            if (bytes_read < 0) {
                int errnum = 0;
                const char* errmsg = gzerror(gz, &errnum);
                gzclose(gz);
                throw std::runtime_error("Error decompressing gzip file " + filepath + ": " + (errmsg ? errmsg : "unknown"));
            }
            if (bytes_read == 0) {
                break;
            }
            buffer.append(chunk.data(), static_cast<size_t>(bytes_read));
        }
        gzclose(gz);
        return buffer;
    }

    // Direct uncompressed file reading
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer;
    if (size > 0) {
        buffer.resize(static_cast<size_t>(size));
        if (!file.read(&buffer[0], size)) {
            throw std::runtime_error("Failed to read file: " + filepath);
        }
    }
    return buffer;
}

std::pair<std::map<std::string, std::string>, std::map<std::string, ScanReplacementInfo>>
build_scan_replacement_map(const Library& library) {
    std::map<std::string, std::string> mapping;
    std::map<std::string, ScanReplacementInfo> details;

    std::map<std::string, const Cell*> scan_cells;
    for (const auto& [name, cell] : library.cells) {
        if (cell.is_scan_cell() || cell.cell_class == CellClass::SCAN_CELL) {
            scan_cells[name] = &cell;
        }
    }
    if (scan_cells.empty()) {
        return {mapping, details};
    }

    std::vector<const Cell*> reg_cells;
    for (const auto& [name, cell] : library.cells) {
        if (!cell.is_sequential() && cell.cell_class != CellClass::SEQUENTIAL) {
            continue;
        }
        if (cell.is_scan_cell() || cell.cell_class == CellClass::SCAN_CELL) {
            continue;
        }
        std::string nlower = name;
        for (char& ch : nlower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (nlower.find("_dl") != std::string::npos ||
            nlower.find("latch") != std::string::npos ||
            nlower.find("lat") != std::string::npos) {
            continue;
        }
        reg_cells.push_back(&cell);
    }

    for (const auto* c : reg_cells) {
        const std::string& cname = c->name;
        const Cell* matched_scan = nullptr;

        std::vector<std::string> candidates;
        // Pattern 1: SkyWater style __df -> __sdf, dfx -> dfs
        size_t p_df = cname.find("__df");
        if (p_df != std::string::npos) {
            std::string cand = cname;
            cand.replace(p_df, 4, "__sdf");
            candidates.push_back(cand);
        }
        size_t p_dfx = cname.find("dfx");
        if (p_dfx != std::string::npos) {
            std::string cand = cname;
            cand.replace(p_dfx, 3, "dfs");
            candidates.push_back(cand);
        }
        // Pattern 2: S / s prefix: DFF_X1 -> SDFF_X1, dff_x1 -> sdff_x1
        candidates.push_back("S" + cname);
        candidates.push_back("s" + cname);
        candidates.push_back("S_" + cname);
        candidates.push_back("s_" + cname);
        candidates.push_back("SCAN_" + cname);
        candidates.push_back("scan_" + cname);
        // Pattern 3: DFF -> SDFF
        size_t p_DFF = cname.find("DFF");
        if (p_DFF != std::string::npos) {
            std::string cand = cname;
            cand.replace(p_DFF, 3, "SDFF");
            candidates.push_back(cand);
        }
        size_t p_dff = cname.find("dff");
        if (p_dff != std::string::npos) {
            std::string cand = cname;
            cand.replace(p_dff, 3, "sdff");
            candidates.push_back(cand);
        }

        for (const auto& cand_name : candidates) {
            auto it = scan_cells.find(cand_name);
            if (it != scan_cells.end()) {
                matched_scan = it->second;
                break;
            }
        }

        // If not found by name pattern, search by footprint / drive strength
        if (!matched_scan && !c->cell_footprint.empty()) {
            std::string footprint = c->cell_footprint;
            for (char& ch : footprint) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            // Extract trailing drive suffix e.g. _1, _x2, _4
            std::string drive = "";
            size_t us = cname.find_last_of('_');
            if (us != std::string::npos && us + 1 < cname.size()) {
                drive = cname.substr(us);
                for (char& ch : drive) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }

            for (const auto& [s_name, s_cell] : scan_cells) {
                std::string s_fp = s_cell->cell_footprint;
                for (char& ch : s_fp) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

                std::string rep_fp = footprint;
                size_t p_d = rep_fp.find("dff");
                if (p_d != std::string::npos) rep_fp.replace(p_d, 3, "sdff");

                if (s_fp == footprint || s_fp == ("s" + footprint) || s_fp == rep_fp) {
                    if (!drive.empty()) {
                        std::string s_lower = s_name;
                        for (char& ch : s_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        if (s_lower.find(drive) != std::string::npos) {
                            matched_scan = s_cell;
                            break;
                        }
                    } else {
                        matched_scan = s_cell;
                        break;
                    }
                }
            }
        }

        if (matched_scan) {
            mapping[cname] = matched_scan->name;
            std::string si = "SI";
            std::string se = "SE";
            std::string so = "Q";
            std::string di = "D";
            std::string clk = "CLK";

            if (matched_scan->test_cell) {
                const auto& tc = *matched_scan->test_cell;
                if (!tc.scan_in.empty()) si = tc.scan_in;
                if (!tc.scan_enable.empty()) se = tc.scan_enable;
                if (!tc.scan_out.empty()) so = tc.scan_out;
                if (!tc.data_in.empty()) di = tc.data_in;
                if (!tc.clock.empty()) clk = tc.clock;
            }

            // Inspect pins if default SI/SE/SO not verified
            if (!matched_scan->test_cell) {
                for (const auto& [pname, pin] : matched_scan->pins) {
                    std::string pu = pname;
                    for (char& ch : pu) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    if (pu == "SI" || pu == "SCD" || pu == "TI") si = pname;
                    else if (pu == "SE" || pu == "SCE" || pu == "TE") se = pname;
                    else if (pu == "SO" || pu == "Q" || pu == "QN") so = pname;
                    else if (pu == "D" || pu == "DATA") di = pname;
                    else if (pin.is_clock) clk = pname;
                }
            }

            details[cname] = ScanReplacementInfo{
                .non_scan_name = cname,
                .scan_name = matched_scan->name,
                .scan_in = si,
                .scan_enable = se,
                .scan_out = so,
                .data_in = di,
                .clock = clk
            };
        }
    }

    return {mapping, details};
}

} // namespace vajra::liberty
