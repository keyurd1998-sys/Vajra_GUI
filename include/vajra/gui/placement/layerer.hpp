#pragma once

#include "vajra/gui/placement/placement_models.hpp"

namespace vajra::gui {

class LayerAssigner {
public:
    static int assign_layers(PlacementGraph& graph);
};

} // namespace vajra::gui
