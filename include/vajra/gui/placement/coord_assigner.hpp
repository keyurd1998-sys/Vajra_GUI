#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/placement/placement_models.hpp"

namespace vajra::gui {

class CoordinateAssigner {
public:
    static Rect assign_coordinates(PlacementGraph& graph,
                                   double col_spacing = 150.0,
                                   double row_spacing = 50.0,
                                   bool center_align = true);
};

} // namespace vajra::gui
