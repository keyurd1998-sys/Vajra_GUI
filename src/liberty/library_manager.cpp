#include "vajra/liberty/library_manager.hpp"
#include "vajra/liberty/loader.hpp"

#include <chrono>
#include <filesystem>

namespace vajra::liberty {

bool LibraryManager::load_library(const std::string& filepath,
                                  const std::string& alias,
                                  bool is_target,
                                  bool is_link,
                                  LibraryLoadStats* out_stats,
                                  std::string* out_err) {
    try {
        auto start = std::chrono::high_resolution_clock::now();
        Parser parser;
        Library lib = parser.parse_file(filepath);
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::string reg_name = alias.empty() ? lib.name : alias;
        if (reg_name.empty()) {
            reg_name = filepath;
        }

        size_t pin_count = 0;
        size_t arc_count = 0;
        for (const auto& [cname, cell] : lib.cells) {
            pin_count += cell.pins.size();
            for (const auto& [pname, pin] : cell.pins) {
                arc_count += pin.timing_arcs.size();
            }
        }

        if (out_stats) {
            out_stats->name = reg_name;
            out_stats->cell_count = lib.cells.size();
            out_stats->pin_count = pin_count;
            out_stats->arc_count = arc_count;
            out_stats->elapsed_ms = elapsed_ms;
        }

        libraries_[reg_name] = std::move(lib);
        library_filepaths_[reg_name] = filepath;

        // Build scan replacement map
        auto [smap, sdetails] = build_scan_replacement_map(libraries_[reg_name]);
        scan_replacement_maps_[reg_name] = std::move(smap);
        scan_replacement_details_[reg_name] = std::move(sdetails);

        if (is_target || target_library_name_.empty()) {
            target_library_name_ = reg_name;
        }

        if (is_link) {
            add_link_library(reg_name);
        }

        return true;
    } catch (const std::exception& e) {
        if (out_err) {
            *out_err = e.what();
        }
        return false;
    }
}

const Library* LibraryManager::get_library(const std::string& name) const {
    auto it = libraries_.find(name);
    if (it != libraries_.end()) return &it->second;

    for (const auto& [k, lib] : libraries_) {
        if (lib.name == name) return &lib;
        std::filesystem::path p(k);
        if (p.filename().string() == name || p.stem().string() == name) return &lib;
    }
    return nullptr;
}

Library* LibraryManager::get_library(const std::string& name) {
    auto it = libraries_.find(name);
    if (it != libraries_.end()) return &it->second;

    for (auto& [k, lib] : libraries_) {
        if (lib.name == name) return &lib;
        std::filesystem::path p(k);
        if (p.filename().string() == name || p.stem().string() == name) return &lib;
    }
    return nullptr;
}

const Library* LibraryManager::get_target_library() const {
    if (target_library_name_.empty()) return nullptr;
    return get_library(target_library_name_);
}

void LibraryManager::set_target_library(const std::string& name) {
    target_library_name_ = name;
}

void LibraryManager::add_link_library(const std::string& name) {
    for (const auto& existing : link_library_names_) {
        if (existing == name) return;
    }
    link_library_names_.push_back(name);
}

void LibraryManager::set_link_libraries(const std::vector<std::string>& names) {
    link_library_names_ = names;
}

const Cell* LibraryManager::find_cell(const std::string& cell_name, const std::string& lib_hint) const {
    return find_cell_with_lib(cell_name, lib_hint).first;
}

std::pair<const Cell*, const Library*> LibraryManager::find_cell_with_lib(const std::string& cell_name, const std::string& lib_hint) const {
    if (!lib_hint.empty()) {
        const auto* lib = get_library(lib_hint);
        if (lib) {
            const auto* c = lib->find_cell(cell_name);
            if (c) return {c, lib};
        }
    }

    if (!target_library_name_.empty()) {
        const auto* lib = get_library(target_library_name_);
        if (lib) {
            const auto* c = lib->find_cell(cell_name);
            if (c) return {c, lib};
        }
    }

    for (const auto& link_name : link_library_names_) {
        const auto* lib = get_library(link_name);
        if (lib) {
            const auto* c = lib->find_cell(cell_name);
            if (c) return {c, lib};
        }
    }

    for (const auto& [name, lib] : libraries_) {
        const auto* c = lib.find_cell(cell_name);
        if (c) return {c, &lib};
    }

    return {nullptr, nullptr};
}

const std::map<std::string, std::string>& LibraryManager::get_scan_replacement_map(const std::string& lib_name) const {
    static const std::map<std::string, std::string> empty_map;
    if (!lib_name.empty()) {
        auto it = scan_replacement_maps_.find(lib_name);
        if (it != scan_replacement_maps_.end()) return it->second;
    } else if (!target_library_name_.empty()) {
        auto it = scan_replacement_maps_.find(target_library_name_);
        if (it != scan_replacement_maps_.end()) return it->second;
    } else if (!scan_replacement_maps_.empty()) {
        return scan_replacement_maps_.begin()->second;
    }
    return empty_map;
}

const std::map<std::string, ScanReplacementInfo>& LibraryManager::get_scan_replacement_details(const std::string& lib_name) const {
    static const std::map<std::string, ScanReplacementInfo> empty_map;
    if (!lib_name.empty()) {
        auto it = scan_replacement_details_.find(lib_name);
        if (it != scan_replacement_details_.end()) return it->second;
    } else if (!target_library_name_.empty()) {
        auto it = scan_replacement_details_.find(target_library_name_);
        if (it != scan_replacement_details_.end()) return it->second;
    } else if (!scan_replacement_details_.empty()) {
        return scan_replacement_details_.begin()->second;
    }
    return empty_map;
}

std::string LibraryManager::get_library_filepath(const std::string& lib_name) const {
    auto it = library_filepaths_.find(lib_name);
    if (it != library_filepaths_.end()) return it->second;
    const auto* lib = get_library(lib_name);
    if (lib && !lib->filepath.empty()) return lib->filepath;
    return "N/A";
}

void LibraryManager::clear() {
    libraries_.clear();
    target_library_name_.clear();
    link_library_names_.clear();
    library_filepaths_.clear();
    scan_replacement_maps_.clear();
    scan_replacement_details_.clear();
}

} // namespace vajra::liberty
