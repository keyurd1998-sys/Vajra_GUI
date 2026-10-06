#pragma once

#include "vajra/gui/models.hpp"
#include <string>
#include <vector>
#include <cmath>

namespace vajra::gui {

enum class SegmentOrientation {
    HORIZONTAL,
    VERTICAL
};

enum class PinExitDirection {
    WEST,   // Signal enters/exits left flank
    EAST,   // Signal enters/exits right flank
    SOUTH,  // Signal enters/exits bottom flank
    NORTH   // Signal enters/exits top flank
};

struct PinLocation {
    std::string node_id;
    std::string pin_name;
    double x{0.0};
    double y{0.0};
    PinExitDirection direction{PinExitDirection::EAST};
    bool is_inverted{false};
    std::string role; // "INPUT", "OUTPUT", "CLOCK", "RESET", etc.
};

struct RouteSegment {
    Point p1;
    Point p2;
    SegmentOrientation orientation{SegmentOrientation::HORIZONTAL};
    int track_id{0};
    bool is_stub{false};

    bool is_orthogonal() const {
        return (std::abs(p1.x - p2.x) < 1e-4) || (std::abs(p1.y - p2.y) < 1e-4);
    }

    bool is_horizontal() const {
        return std::abs(p1.y - p2.y) < 1e-4;
    }

    bool is_vertical() const {
        return std::abs(p1.x - p2.x) < 1e-4;
    }
};

struct SolderDot {
    Point pos;
    std::string net_name;
    double radius{3.5};
};

struct HFNStub {
    std::string net_name;
    PinLocation pin_loc;
    Point start;
    Point end;
    std::string label;
    bool is_driver{false};
    std::string arrow_direction; // "LEFT", "RIGHT", "UP", "DOWN"
};

struct NetRoute {
    std::string net_name;
    std::vector<RouteSegment> segments;
    std::vector<SolderDot> solder_dots;
    std::vector<HFNStub> hfn_stubs;
    bool is_high_fanout{false};
    bool is_clock{false};
    bool is_reset{false};
    PinLocation driver_pin;
    std::vector<PinLocation> sink_pins;
};

struct RoutingResult {
    std::string module_name;
    std::vector<NetRoute> routes;
    int total_segments{0};
    int total_solder_dots{0};
    int total_bends{0};
    int decoupled_hfn_count{0};
    int regular_routed_count{0};
    bool is_strictly_orthogonal{true};
    Rect bbox{0.0, 0.0, 0.0, 0.0};
};

} // namespace vajra::gui
