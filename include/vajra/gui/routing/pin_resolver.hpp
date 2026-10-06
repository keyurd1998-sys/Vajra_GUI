#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/placement/placement_models.hpp"
#include "vajra/gui/routing/router_models.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace vajra::gui {

class PinResolver {
public:
    PinResolver(const PlacementGraph& graph, const SchematicModule& module);

    const PinLocation* get_pin(const std::string& node_id, const std::string& pin_name) const;
    std::vector<PinLocation> get_node_pins(const std::string& node_id) const;

private:
    void resolve_all_pins(const PlacementGraph& graph, const SchematicModule& module);
    void resolve_dff_pins(const PlacementNode& node, const SchematicCell* cell);
    void resolve_module_pins(const PlacementNode& node, const SchematicCell* cell);
    void resolve_gate_pins(const PlacementNode& node, const SchematicCell* cell);

    std::unordered_map<std::string, PinLocation> pin_map_; // "node_id:pin_name" -> PinLocation
    std::unordered_map<std::string, std::vector<PinLocation>> node_pins_; // node_id -> pins
};

} // namespace vajra::gui
