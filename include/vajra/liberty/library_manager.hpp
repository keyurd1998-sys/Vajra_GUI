#pragma once

#include "vajra/liberty/models.hpp"
#include "vajra/liberty/parser.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vajra::liberty {

struct LibraryLoadStats {
    std::string name;
    size_t cell_count{0};
    size_t pin_count{0};
    size_t arc_count{0};
    double elapsed_ms{0.0};
};

class LibraryManager {
public:
    LibraryManager() = default;

    // Loads and registers a library (.lib or gzip-compressed .lib.gz)
    bool load_library(const std::string& filepath,
                      const std::string& alias = "",
                      bool is_target = false,
                      bool is_link = false,
                      LibraryLoadStats* out_stats = nullptr,
                      std::string* out_err = nullptr);

    const Library* get_library(const std::string& name) const;
    Library* get_library(const std::string& name);

    const Library* get_target_library() const;
    void set_target_library(const std::string& name);
    const std::string& get_target_library_name() const { return target_library_name_; }

    const std::vector<std::string>& get_link_libraries() const { return link_library_names_; }
    void add_link_library(const std::string& name);
    void set_link_libraries(const std::vector<std::string>& names);

    const std::map<std::string, Library>& get_libraries() const { return libraries_; }

    // Scan replacement maps & details
    const std::map<std::string, std::string>& get_scan_replacement_map(const std::string& lib_name = "") const;
    const std::map<std::string, ScanReplacementInfo>& get_scan_replacement_details(const std::string& lib_name = "") const;

    // File path tracking
    const std::map<std::string, std::string>& get_library_filepaths() const { return library_filepaths_; }
    std::string get_library_filepath(const std::string& lib_name) const;

    // Searches for a standard cell: checks lib_hint, target_library, link_libraries, then any loaded library
    const Cell* find_cell(const std::string& cell_name, const std::string& lib_hint = "") const;
    std::pair<const Cell*, const Library*> find_cell_with_lib(const std::string& cell_name, const std::string& lib_hint = "") const;

    void clear();

private:
    std::map<std::string, Library> libraries_;
    std::string target_library_name_;
    std::vector<std::string> link_library_names_;
    std::map<std::string, std::string> library_filepaths_;
    std::map<std::string, std::map<std::string, std::string>> scan_replacement_maps_;
    std::map<std::string, std::map<std::string, ScanReplacementInfo>> scan_replacement_details_;
};

} // namespace vajra::liberty
