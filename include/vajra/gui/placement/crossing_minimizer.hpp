#pragma once

#include "vajra/gui/placement/placement_models.hpp"

namespace vajra::gui {

class CrossingMinimizer {
public:
    static void minimize(PlacementGraph& graph, int iterations = 4);
};

} // namespace vajra::gui
