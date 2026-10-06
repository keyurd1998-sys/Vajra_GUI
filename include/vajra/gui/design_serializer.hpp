#pragma once

#include "vajra/gui/models.hpp"
#include <string>

namespace vajra::gui {

class DesignSerializer {
public:
    static bool save_to_file(const SchematicDesign& design, const std::string& filepath);
    static bool load_from_file(const std::string& filepath, SchematicDesign& design);
};

} // namespace vajra::gui
