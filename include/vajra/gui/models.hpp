#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace vajra::gui {

enum class NodeType {
    PRIMARY_INPUT,
    PRIMARY_OUTPUT,
    GATE,
    DFF,
    LATCH,
    MODULE,
    TIE_HI,
    TIE_LO
};

enum class PinDirection {
    INPUT,
    OUTPUT,
    INOUT
};

enum class PinExitSide {
    WEST,   // Left side (Input pins)
    EAST,   // Right side (Output pins)
    SOUTH,  // Bottom side (Clock / Reset)
    NORTH   // Top side (Set / Preset / Enable)
};

struct Point {
    double x{0.0};
    double y{0.0};

    bool operator==(const Point& other) const {
        return std::abs(x - other.x) < 1e-4 && std::abs(y - other.y) < 1e-4;
    }
};

struct Rect {
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};

    double right() const { return x + width; }
    double bottom() const { return y + height; }
    Point center() const { return {x + width / 2.0, y + height / 2.0}; }
};

struct SchematicPin {
    std::string name;
    std::string owner_node_id;
    PinDirection direction{PinDirection::INPUT};
    PinExitSide exit_side{PinExitSide::WEST};
    Point local_pos{0.0, 0.0};   // Relative to owner cell bounding box
    Point scene_pos{0.0, 0.0};   // Absolute canvas coordinate
    std::string net_name;
    bool is_clock{false};
    bool is_reset{false};
    bool is_inverted{false};
    int bit_index{-1};
};

struct SchematicCell {
    std::string id;
    std::string name;
    std::string cell_type;
    NodeType type{NodeType::GATE};
    Rect bbox{0.0, 0.0, 80.0, 50.0};
    std::vector<SchematicPin> pins;
    bool is_submodule{false};
    std::string submodule_target;
    std::unordered_map<std::string, std::string> attributes;

    const SchematicPin* get_pin(const std::string& pin_name) const {
        for (const auto& pin : pins) {
            if (pin.name == pin_name) return &pin;
        }
        return nullptr;
    }
};

struct SchematicNet {
    std::string name;
    std::string driver_pin; // Format: "cell_id:pin_name" or "PORT:port_name"
    std::vector<std::string> sink_pins; // Format: "cell_id:pin_name" or "PORT:port_name"
    bool is_clock{false};
    bool is_reset{false};
    bool is_bus{false};
    int width{1};
};

struct SchematicModule {
    std::string name;
    std::unordered_map<std::string, SchematicCell> cells;
    std::unordered_map<std::string, SchematicPin> ports;
    std::unordered_map<std::string, SchematicNet> nets;
    std::unordered_map<std::string, std::string> attributes;

    const SchematicCell* get_cell(const std::string& cell_id) const {
        auto it = cells.find(cell_id);
        return it != cells.end() ? &it->second : nullptr;
    }

    const SchematicPin* get_port(const std::string& port_name) const {
        auto it = ports.find(port_name);
        return it != ports.end() ? &it->second : nullptr;
    }
};

struct SchematicDesign {
    std::string top_module;
    std::unordered_map<std::string, SchematicModule> modules;

    const SchematicModule* get_module(const std::string& name) const {
        auto it = modules.find(name);
        return it != modules.end() ? &it->second : nullptr;
    }

    const SchematicModule* get_top_module() const {
        return get_module(top_module);
    }
};

} // namespace vajra::gui
