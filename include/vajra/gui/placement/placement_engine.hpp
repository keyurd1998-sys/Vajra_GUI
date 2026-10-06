#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/placement/placement_models.hpp"
#include "vajra/gui/placement/cycle_breaker.hpp"
#include "vajra/gui/placement/layerer.hpp"
#include "vajra/gui/placement/crossing_minimizer.hpp"
#include "vajra/gui/placement/coord_assigner.hpp"

namespace vajra::gui {

struct PlacementResult {
    std::string module_name;
    PlacementGraph graph;
    Rect bbox;
    int num_ranks{0};
    int total_nodes{0};
    int total_edges{0};
    int feedback_edges{0};
};

class PlacementEngine {
public:
    static PlacementResult run(const SchematicModule& module,
                              int num_crossing_iterations = 4,
                              double col_spacing = 180.0,
                              double row_spacing = 60.0,
                              bool center_align = true,
                              bool decouple_dff = true);
};

} // namespace vajra::gui
