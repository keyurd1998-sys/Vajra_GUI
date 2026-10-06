#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/placement/placement_engine.hpp"
#include "vajra/gui/routing/router_models.hpp"

#include <vector>

namespace vajra::gui {

class OrthogonalRouter {
public:
    static std::vector<NetRoute> route(const SchematicModule& module,
                                       const PlacementResult& placement,
                                       int hfn_threshold = 12);

    static RoutingResult route_all(const SchematicModule& module,
                                  const PlacementResult& placement,
                                  int hfn_threshold = 12);
};

} // namespace vajra::gui
