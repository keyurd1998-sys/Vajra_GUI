#include "vajra/gui/routing/pin_resolver.hpp"

#include <algorithm>
#include <cctype>

namespace vajra::gui {

namespace {

std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

std::string make_key(const std::string& node_id, const std::string& pin_name) {
    return node_id + ":" + pin_name;
}

} // anonymous namespace

PinResolver::PinResolver(const PlacementGraph& graph, const SchematicModule& module) {
    resolve_all_pins(graph, module);
}

const PinLocation* PinResolver::get_pin(const std::string& node_id, const std::string& pin_name) const {
    // 1. Direct exact lookup
    std::string key = make_key(node_id, pin_name);
    auto it = pin_map_.find(key);
    if (it != pin_map_.end()) return &it->second;

    // 2. Port prefix handling: PORT:foo or port:foo
    if (node_id == "PORT" || node_id == "port") {
        std::string pkey = make_key("port:" + pin_name, pin_name);
        it = pin_map_.find(pkey);
        if (it != pin_map_.end()) return &it->second;
        pkey = make_key("port:" + pin_name, "IN");
        it = pin_map_.find(pkey);
        if (it != pin_map_.end()) return &it->second;
        pkey = make_key("port:" + pin_name, "OUT");
        it = pin_map_.find(pkey);
        if (it != pin_map_.end()) return &it->second;
    }

    // 3. Fallback: single pin on port node
    auto it_np = node_pins_.find(node_id);
    if (it_np != node_pins_.end() && !it_np->second.empty()) {
        if (node_id.rfind("port:", 0) == 0) {
            return &it_np->second.front();
        }
        // Case-insensitive pin match
        std::string l_pin = to_lower(pin_name);
        for (const auto& pl : it_np->second) {
            if (to_lower(pl.pin_name) == l_pin) {
                return &pl;
            }
        }
    }

    return nullptr;
}

std::vector<PinLocation> PinResolver::get_node_pins(const std::string& node_id) const {
    auto it = node_pins_.find(node_id);
    return it != node_pins_.end() ? it->second : std::vector<PinLocation>{};
}

void PinResolver::resolve_all_pins(const PlacementGraph& graph, const SchematicModule& module) {
    pin_map_.clear();
    node_pins_.clear();

    for (const auto& [node_id, node] : graph.nodes) {
        if (node.kind == "PRIMARY_INPUT") {
            // Signal emerges to the East towards the circuit
            PinLocation loc;
            loc.node_id = node_id;
            loc.pin_name = node.name;
            loc.x = node.x + node.width;
            loc.y = node.y + node.height / 2.0;
            loc.direction = PinExitDirection::EAST;
            loc.role = "OUTPUT";

            node_pins_[node_id].push_back(loc);
            pin_map_[make_key(node_id, node.name)] = loc;
            pin_map_[make_key(node_id, "OUT")] = loc;
            pin_map_[make_key(node_id, "Y")] = loc;
            pin_map_[make_key(node_id, "")] = loc;

        } else if (node.kind == "PRIMARY_OUTPUT") {
            // Signal enters from the West from the circuit
            PinLocation loc;
            loc.node_id = node_id;
            loc.pin_name = "IN";
            loc.x = node.x;
            loc.y = node.y + node.height / 2.0;
            loc.direction = PinExitDirection::WEST;
            loc.role = "INPUT";

            node_pins_[node_id].push_back(loc);
            pin_map_[make_key(node_id, "IN")] = loc;
            pin_map_[make_key(node_id, node.name)] = loc;
            pin_map_[make_key(node_id, "A")] = loc;
            pin_map_[make_key(node_id, "")] = loc;

        } else if (node.kind == "MODULE") {
            const auto* cell = module.get_cell(node_id);
            resolve_module_pins(node, cell);

        } else if (node.kind == "DFF" || node.kind == "SDFF" || node.kind == "LATCH") {
            const auto* cell = module.get_cell(node_id);
            resolve_dff_pins(node, cell);

        } else {
            const auto* cell = module.get_cell(node_id);
            resolve_gate_pins(node, cell);
        }
    }
}

void PinResolver::resolve_module_pins(const PlacementNode& node, const SchematicCell* cell) {
    std::vector<std::string> in_pins;
    std::vector<std::string> out_pins;

    if (cell) {
        for (const auto& pin : cell->pins) {
            if (pin.direction == PinDirection::INPUT) {
                in_pins.push_back(pin.name);
            } else {
                out_pins.push_back(pin.name);
            }
        }
    }

    double header_height = 24.0;
    double footer_height = 18.0;
    double pin_pitch = 20.0;

    double in_start_y = header_height + (node.height - header_height - footer_height - in_pins.size() * pin_pitch) / 2.0 + pin_pitch / 2.0;
    for (size_t i = 0; i < in_pins.size(); ++i) {
        double y = node.y + in_start_y + (i * pin_pitch);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = in_pins[i];
        loc.x = node.x;
        loc.y = y;
        loc.direction = PinExitDirection::WEST;
        loc.role = "INPUT";

        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, in_pins[i])] = loc;
    }

    double out_start_y = header_height + (node.height - header_height - footer_height - out_pins.size() * pin_pitch) / 2.0 + pin_pitch / 2.0;
    for (size_t i = 0; i < out_pins.size(); ++i) {
        double y = node.y + out_start_y + (i * pin_pitch);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = out_pins[i];
        loc.x = node.x + node.width;
        loc.y = y;
        loc.direction = PinExitDirection::EAST;
        loc.role = "OUTPUT";

        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, out_pins[i])] = loc;
    }
}

void PinResolver::resolve_dff_pins(const PlacementNode& node, const SchematicCell* cell) {
    std::string clk_pin;
    std::string rst_pin;
    std::string pre_pin;
    std::vector<std::string> data_pins;
    std::vector<std::string> out_pins;

    if (cell) {
        for (const auto& p : cell->pins) {
            std::string lp = to_lower(p.name);
            if (p.is_clock || lp == "c" || lp == "clk" || lp == "cp" || lp == "gate" || lp == "g") {
                clk_pin = p.name;
            } else if (p.is_reset || lp.find("rst") != std::string::npos || lp.find("reset") != std::string::npos || lp.find("clr") != std::string::npos || lp.find("clear") != std::string::npos) {
                rst_pin = p.name;
            } else if (p.exit_side == PinExitSide::NORTH || lp.find("pre") != std::string::npos || (lp.find("set") != std::string::npos && lp.find("reset") == std::string::npos)) {
                pre_pin = p.name;
            } else if (p.direction == PinDirection::OUTPUT) {
                out_pins.push_back(p.name);
            } else {
                data_pins.push_back(p.name);
            }
        }
    } else {
        clk_pin = "CLK";
    }

    if (data_pins.empty() && !cell) data_pins.push_back("D");
    if (out_pins.empty() && !cell) out_pins.push_back("Q");

    // Order data inputs predictably: D first, then SI, then SE
    std::sort(data_pins.begin(), data_pins.end(), [](const std::string& a, const std::string& b) {
        auto rank = [](const std::string& s) {
            std::string ls = s;
            std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
            if (ls == "d" || ls == "din" || ls == "data") return 0;
            if (ls == "si" || ls == "scd" || ls == "ti") return 1;
            if (ls == "se" || ls == "sce" || ls == "te") return 2;
            return 3;
        };
        int ra = rank(a);
        int rb = rank(b);
        if (ra != rb) return ra < rb;
        return a < b;
    });

    // Order outputs: Q first, QN second
    std::sort(out_pins.begin(), out_pins.end(), [](const std::string& a, const std::string& b) {
        auto rank = [](const std::string& s) {
            std::string ls = s;
            std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
            if (ls == "q" || ls == "out") return 0;
            if (ls == "qn" || ls == "q_n" || ls == "qb") return 1;
            return 2;
        };
        int ra = rank(a);
        int rb = rank(b);
        if (ra != rb) return ra < rb;
        return a < b;
    });

    // North preset pin (if present)
    if (!pre_pin.empty()) {
        PinLocation p_pre;
        p_pre.node_id = node.id;
        p_pre.pin_name = pre_pin;
        p_pre.x = node.x + node.width / 2.0;
        p_pre.y = node.y;
        p_pre.direction = PinExitDirection::NORTH;
        p_pre.role = "PRESET";
        node_pins_[node.id].push_back(p_pre);
        pin_map_[make_key(node.id, pre_pin)] = p_pre;
    }

    // South clock / enable pin
    if (!clk_pin.empty()) {
        PinLocation p_clk;
        p_clk.node_id = node.id;
        p_clk.pin_name = clk_pin;
        p_clk.x = rst_pin.empty() ? (node.x + node.width / 2.0) : (node.x + 28.0);
        p_clk.y = node.y + node.height;
        p_clk.direction = PinExitDirection::SOUTH;
        p_clk.role = "CLOCK";
        node_pins_[node.id].push_back(p_clk);
        pin_map_[make_key(node.id, clk_pin)] = p_clk;
    }

    // South reset pin (only if present!)
    if (!rst_pin.empty()) {
        PinLocation p_rst;
        p_rst.node_id = node.id;
        p_rst.pin_name = rst_pin;
        p_rst.x = clk_pin.empty() ? (node.x + node.width / 2.0) : (node.x + node.width - 28.0);
        p_rst.y = node.y + node.height;
        p_rst.direction = PinExitDirection::SOUTH;
        p_rst.role = "RESET";
        node_pins_[node.id].push_back(p_rst);
        pin_map_[make_key(node.id, rst_pin)] = p_rst;
    }

    // West inputs
    for (size_t i = 0; i < data_pins.size(); ++i) {
        double y = node.y + (node.height * (i + 1.0)) / (data_pins.size() + 1.0);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = data_pins[i];
        loc.x = node.x;
        loc.y = y;
        loc.direction = PinExitDirection::WEST;
        loc.role = "DATA";
        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, data_pins[i])] = loc;
    }

    // East outputs
    for (size_t i = 0; i < out_pins.size(); ++i) {
        double y = node.y + (node.height * (i + 1.0)) / (out_pins.size() + 1.0);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = out_pins[i];
        loc.x = node.x + node.width;
        loc.y = y;
        loc.direction = PinExitDirection::EAST;
        loc.role = "OUTPUT";
        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, out_pins[i])] = loc;
    }
}

void PinResolver::resolve_gate_pins(const PlacementNode& node, const SchematicCell* cell) {
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> south_pins;

    if (cell) {
        for (const auto& p : cell->pins) {
            std::string lp = to_lower(p.name);
            if (p.is_clock || p.is_reset || lp == "s" || lp == "sel") {
                south_pins.push_back(p.name);
            } else if (p.direction == PinDirection::OUTPUT) {
                outputs.push_back(p.name);
            } else {
                inputs.push_back(p.name);
            }
        }
    }

    if (inputs.empty() && outputs.empty() && south_pins.empty()) {
        inputs = {"A", "B"};
        outputs = {"Y"};
    }

    // South pins
    for (size_t i = 0; i < south_pins.size(); ++i) {
        double x = node.x + (node.width * (i + 1.0)) / (south_pins.size() + 1.0);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = south_pins[i];
        loc.x = x;
        loc.y = node.y + node.height;
        loc.direction = PinExitDirection::SOUTH;
        loc.role = "CONTROL";
        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, south_pins[i])] = loc;
    }

    // West inputs
    for (size_t i = 0; i < inputs.size(); ++i) {
        double y = node.y + (node.height * (i + 1.0)) / (inputs.size() + 1.0);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = inputs[i];
        loc.x = node.x;
        loc.y = y;
        loc.direction = PinExitDirection::WEST;
        loc.role = "INPUT";
        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, inputs[i])] = loc;
    }

    // East outputs
    for (size_t i = 0; i < outputs.size(); ++i) {
        double y = node.y + (node.height * (i + 1.0)) / (outputs.size() + 1.0);
        PinLocation loc;
        loc.node_id = node.id;
        loc.pin_name = outputs[i];
        loc.x = node.x + node.width;
        loc.y = y;
        loc.direction = PinExitDirection::EAST;
        loc.role = "OUTPUT";
        node_pins_[node.id].push_back(loc);
        pin_map_[make_key(node.id, outputs[i])] = loc;
    }
}

} // namespace vajra::gui
