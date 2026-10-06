#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/placement/placement_models.hpp"

namespace vajra::gui {

class CycleBreaker {
public:
    static PlacementGraph build_dag(const SchematicModule& module, bool decouple_dff = true);
};

} // namespace vajra::gui
