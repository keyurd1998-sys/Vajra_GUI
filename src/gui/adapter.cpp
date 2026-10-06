#include "vajra/gui/adapter.hpp"
#include "vajra/gui/symbol_classifier.hpp"
#include "vajra/liberty/library_manager.hpp"
#include "vajra/liberty/models.hpp"
#include "vajra/core/logger.hpp"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

#include <algorithm>
#include <cctype>

USING_YOSYS_NAMESPACE

namespace vajra::gui {

std::string RTLILAdapter::sigspec_to_net_name(const RTLIL::SigSpec& sig) {
    if (sig.empty()) {
        return "";
    }
    if (sig.is_wire()) {
        return RTLIL::unescape_id(sig.as_wire()->name);
    }
    if (sig.is_chunk()) {
        const auto& chunk = sig.as_chunk();
        if (chunk.wire != nullptr) {
            std::string wname = RTLIL::unescape_id(chunk.wire->name);
            if (chunk.offset == 0 && chunk.width == chunk.wire->width) {
                return wname;
            }
            if (chunk.width == 1) {
                return wname + "[" + std::to_string(chunk.offset) + "]";
            }
            return wname + "[" + std::to_string(chunk.offset + chunk.width - 1) + ":" + std::to_string(chunk.offset) + "]";
        }
    }
    if (sig.size() == 1) {
        const auto& bit = sig[0];
        if (bit.wire != nullptr) {
            std::string wname = RTLIL::unescape_id(bit.wire->name);
            if (bit.wire->width == 1 && bit.offset == 0) {
                return wname;
            }
            return wname + "[" + std::to_string(bit.offset) + "]";
        }
        if (bit.data == RTLIL::State::S0) return "1'b0";
        if (bit.data == RTLIL::State::S1) return "1'b1";
        if (bit.data == RTLIL::State::Sx) return "1'bx";
        if (bit.data == RTLIL::State::Sz) return "1'bz";
    }

    RTLIL::Wire* common_wire = nullptr;
    bool all_same = true;
    for (const auto& bit : sig) {
        if (bit.wire == nullptr) {
            all_same = false;
            break;
        }
        if (common_wire == nullptr) {
            common_wire = bit.wire;
        } else if (common_wire != bit.wire) {
            all_same = false;
            break;
        }
    }
    if (all_same && common_wire != nullptr) {
        std::string wname = RTLIL::unescape_id(common_wire->name);
        if (sig.size() == static_cast<size_t>(common_wire->width)) {
            bool sequential = true;
            for (int i = 0; i < common_wire->width; ++i) {
                if (sig[i].offset != i) {
                    sequential = false;
                    break;
                }
            }
            if (sequential) {
                return wname;
            }
        }
        bool contiguous = true;
        int start_off = sig[0].offset;
        for (size_t i = 1; i < sig.size(); ++i) {
            if (sig[i].offset != start_off + static_cast<int>(i)) {
                contiguous = false;
                break;
            }
        }
        if (contiguous) {
            return wname + "[" + std::to_string(start_off + sig.size() - 1) + ":" + std::to_string(start_off) + "]";
        }
    }

    std::string name = log_signal(sig);
    if (!name.empty() && name[0] == '\\') {
        name = name.substr(1);
    }
    std::string cleaned;
    cleaned.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] == ' ' && i + 1 < name.size() && name[i + 1] == '[') {
            continue;
        }
        cleaned.push_back(name[i]);
    }
    return cleaned;
}

SchematicDesign RTLILAdapter::extract_design(RTLIL::Design* design,
                                            const std::string& top_module_name,
                                            const liberty::LibraryManager* lib_mgr) {
    SchematicDesign result;
    if (!design) return result;

    RTLIL::Module* top_mod = nullptr;
    if (!top_module_name.empty()) {
        top_mod = design->module(RTLIL::escape_id(top_module_name));
    }
    if (!top_mod) {
        top_mod = design->top_module();
    }
    if (!top_mod && design->modules().begin() != design->modules().end()) {
        top_mod = *design->modules().begin();
    }

    if (top_mod) {
        result.top_module = RTLIL::unescape_id(top_mod->name);
    }

    for (auto* mod : design->modules()) {
        if (!mod) continue;
        if (mod != top_mod && (mod->get_bool_attribute(ID::blackbox) || mod->get_bool_attribute(ID::whitebox))) {
            continue;
        }
        std::string mod_name = RTLIL::unescape_id(mod->name);
        result.modules[mod_name] = extract_module(mod, design, lib_mgr);
    }

    return result;
}

SchematicModule RTLILAdapter::extract_module(RTLIL::Module* module,
                                            RTLIL::Design* design,
                                            const liberty::LibraryManager* lib_mgr) {
    SchematicModule target;
    if (!module) return target;

    target.name = RTLIL::unescape_id(module->name);

    // 1. Extract Primary Input and Output Ports
    extract_ports(module, target);

    // 2. Extract Cells (Primitives, Gates, and Submodules)
    extract_cells(module, target, design, lib_mgr);

    // 3. Extract Continuous Assignments (SigSig pairs)
    extract_assigns(module, target);

    // 4. Reconcile Net Connectivity (Drivers and Sinks)
    reconcile_nets(target);

    return target;
}

void RTLILAdapter::extract_ports(RTLIL::Module* module, SchematicModule& target) {
    for (auto* wire : module->wires()) {
        if (wire->port_id <= 0) continue;

        std::string port_name = RTLIL::unescape_id(wire->name);
        SchematicPin port;
        port.name = port_name;
        port.owner_node_id = "PORT:" + port_name;
        port.net_name = port_name;

        std::string lower_name = port_name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lower_name.find("clk") != std::string::npos || lower_name.find("clock") != std::string::npos) {
            port.is_clock = true;
        }
        if (lower_name.find("rst") != std::string::npos || lower_name.find("reset") != std::string::npos) {
            port.is_reset = true;
        }

        if (wire->port_input && !wire->port_output) {
            port.direction = PinDirection::INPUT;
            port.exit_side = PinExitSide::EAST; // Port on left border faces East
        } else if (wire->port_output && !wire->port_input) {
            port.direction = PinDirection::OUTPUT;
            port.exit_side = PinExitSide::WEST; // Port on right border faces West
        } else {
            port.direction = PinDirection::INOUT;
            port.exit_side = PinExitSide::EAST;
        }

        target.ports[port_name] = port;

        if (wire->width > 1) {
            bool has_bit_connections = false;
            for (auto* cell : module->cells()) {
                if (!cell) continue;
                for (const auto& conn : cell->connections()) {
                    for (const auto& bit : conn.second) {
                        if (bit.wire == wire && conn.second.size() < static_cast<size_t>(wire->width)) {
                            has_bit_connections = true;
                            break;
                        }
                    }
                    if (has_bit_connections) break;
                }
                if (has_bit_connections) break;
            }
            if (!has_bit_connections) {
                for (const auto& conn : module->connections()) {
                    for (const auto& bit : conn.first) {
                        if (bit.wire == wire && conn.first.size() < static_cast<size_t>(wire->width)) {
                            has_bit_connections = true;
                            break;
                        }
                    }
                    if (has_bit_connections) break;
                    for (const auto& bit : conn.second) {
                        if (bit.wire == wire && conn.second.size() < static_cast<size_t>(wire->width)) {
                            has_bit_connections = true;
                            break;
                        }
                    }
                    if (has_bit_connections) break;
                }
            }

            if (has_bit_connections) {
                for (int b = 0; b < wire->width; ++b) {
                    std::string bit_port_name = port_name + "[" + std::to_string(b) + "]";
                    SchematicPin bit_port = port;
                    bit_port.name = bit_port_name;
                    bit_port.owner_node_id = "PORT:" + bit_port_name;
                    bit_port.net_name = bit_port_name;
                    bit_port.bit_index = b;
                    target.ports[bit_port_name] = bit_port;
                }
            }
        }
    }
}

void RTLILAdapter::extract_cells(RTLIL::Module* module,
                                SchematicModule& target,
                                RTLIL::Design* design,
                                const liberty::LibraryManager* lib_mgr) {
    for (auto* cell : module->cells()) {
        if (!cell) continue;

        std::string cell_id = RTLIL::unescape_id(cell->name);
        std::string cell_type = RTLIL::unescape_id(cell->type);

        SchematicCell scell;
        scell.id = cell_id;
        scell.name = cell_id;
        scell.cell_type = cell_type;

        // Check if cell is a hierarchical submodule defined in the design
        RTLIL::Module* submod = nullptr;
        if (design) {
            submod = design->module(cell->type);
        }

        bool is_hierarchical = false;
        if (submod != nullptr) {
            // A module is a hierarchical submodule only if it is NOT a blackbox/whitebox standard cell
            // and actually contains child cells/logic.
            if (!submod->get_bool_attribute(ID::blackbox) &&
                !submod->get_bool_attribute(ID::whitebox) &&
                submod->cells().size() > 0) {
                is_hierarchical = true;
            }
        }

        const liberty::Cell* lib_cell = nullptr;
        if (lib_mgr) {
            lib_cell = lib_mgr->find_cell(cell_type);
        }

        std::optional<SymbolClassification> lib_sym_class;

        if (is_hierarchical) {
            scell.is_submodule = true;
            scell.submodule_target = RTLIL::unescape_id(submod->name);
            scell.type = NodeType::MODULE;
            scell.attributes["symbol_gate_type"] = "MODULE";
        } else if (lib_cell != nullptr) {
            // Exact classification and gate symbol derived from loaded Liberty library (.lib)
            auto sym_class = SymbolClassifier::classify_from_liberty(*lib_cell);
            lib_sym_class = sym_class;
            if (sym_class.gate_type == GateType::DFF || sym_class.gate_type == GateType::SDFF) {
                scell.type = NodeType::DFF;
            } else if (sym_class.gate_type == GateType::LATCH) {
                scell.type = NodeType::LATCH;
            } else if (sym_class.gate_type == GateType::MODULE_BOX) {
                scell.type = NodeType::MODULE;
            } else {
                scell.type = NodeType::GATE;
            }
            scell.attributes["symbol_gate_type"] = SymbolClassifier::gate_type_to_string(sym_class.gate_type);
            scell.attributes["symbol_description"] = sym_class.description;
            if (sym_class.is_inverting) scell.attributes["is_inverting"] = "true";
            if (sym_class.is_scan) scell.attributes["is_scan"] = "true";
            if (sym_class.is_latch) scell.attributes["is_latch"] = "true";
            if (sym_class.is_falling_edge) scell.attributes["is_falling_edge"] = "true";
            if (sym_class.has_reset) {
                scell.attributes["has_reset"] = "true";
                scell.attributes["reset_pin"] = sym_class.reset_pin;
                scell.attributes["is_reset_active_low"] = sym_class.is_reset_active_low ? "true" : "false";
            }
            if (sym_class.has_preset) {
                scell.attributes["has_preset"] = "true";
                scell.attributes["preset_pin"] = sym_class.preset_pin;
                scell.attributes["is_preset_active_low"] = sym_class.is_preset_active_low ? "true" : "false";
            }
            if (!sym_class.scan_in_pin.empty()) scell.attributes["scan_in_pin"] = sym_class.scan_in_pin;
            if (!sym_class.scan_enable_pin.empty()) scell.attributes["scan_enable_pin"] = sym_class.scan_enable_pin;
            if (!sym_class.qn_pin.empty()) scell.attributes["qn_pin"] = sym_class.qn_pin;
        } else if (cell_type.starts_with("$_") || cell_type.starts_with("$")) {
            // Internal RTLIL primitive (predefined mathematical primitive)
            auto sym_class = SymbolClassifier::classify(cell_type);
            if (sym_class.gate_type == GateType::DFF || sym_class.gate_type == GateType::SDFF) {
                scell.type = NodeType::DFF;
            } else if (sym_class.gate_type == GateType::LATCH) {
                scell.type = NodeType::LATCH;
            } else {
                scell.type = NodeType::GATE;
            }
            scell.attributes["symbol_gate_type"] = SymbolClassifier::gate_type_to_string(sym_class.gate_type);
            scell.attributes["symbol_description"] = sym_class.description;
            if (sym_class.is_scan) scell.attributes["is_scan"] = "true";
            if (sym_class.is_latch) scell.attributes["is_latch"] = "true";
            if (sym_class.is_falling_edge) scell.attributes["is_falling_edge"] = "true";
            if (sym_class.has_reset) {
                scell.attributes["has_reset"] = "true";
                scell.attributes["reset_pin"] = sym_class.reset_pin;
                scell.attributes["is_reset_active_low"] = sym_class.is_reset_active_low ? "true" : "false";
            }
        } else {
            // Unresolved / unlinked ASIC standard cell or hard macro
            scell.type = NodeType::MODULE;
            scell.attributes["symbol_gate_type"] = "MODULE";
            core::Logger::instance().warning(205,
                "Cell '" + cell_id + "' of type '" + cell_type +
                "' has no Liberty model loaded. Rendering as generic module box.");
        }

        // Extract Pins & Connections
        for (const auto& conn : cell->connections()) {
            std::string pin_name = RTLIL::unescape_id(conn.first);
            std::string net_name = sigspec_to_net_name(conn.second);

            SchematicPin pin;
            pin.name = pin_name;
            pin.owner_node_id = cell_id;
            pin.net_name = net_name;
            if (conn.second.is_chunk()) {
                pin.bit_index = conn.second.as_chunk().offset;
            } else if (!conn.second.empty() && conn.second[0].wire) {
                pin.bit_index = conn.second[0].offset;
            }

            if (lib_cell != nullptr) {
                // Pin direction strictly from Liberty model
                auto lpin_it = lib_cell->pins.find(pin_name);
                if (lpin_it != lib_cell->pins.end()) {
                    const auto& lpin = lpin_it->second;
                    if (lpin.direction == liberty::PinDirection::OUTPUT) {
                        pin.direction = PinDirection::OUTPUT;
                        pin.exit_side = PinExitSide::EAST;
                    } else if (lpin.direction == liberty::PinDirection::INOUT) {
                        pin.direction = PinDirection::INOUT;
                        pin.exit_side = PinExitSide::EAST;
                    } else {
                        pin.direction = PinDirection::INPUT;
                        pin.exit_side = PinExitSide::WEST;
                    }
                    if (lpin.is_clock) {
                        pin.is_clock = true;
                        pin.exit_side = PinExitSide::SOUTH;
                    }
                } else {
                    pin.direction = PinDirection::INPUT;
                    pin.exit_side = PinExitSide::WEST;
                }

                // Pin role and bubbles from Liberty classification
                if (lib_sym_class) {
                    for (const auto& bp : lib_sym_class->bubble_pins) {
                        if (bp == pin_name) {
                            pin.is_inverted = true;
                            break;
                        }
                    }
                    auto r_it = lib_sym_class->pin_roles.find(pin_name);
                    if (r_it != lib_sym_class->pin_roles.end()) {
                        if (r_it->second == PinRole::CLOCK) {
                            pin.is_clock = true;
                            pin.exit_side = PinExitSide::SOUTH;
                        } else if (r_it->second == PinRole::RESET) {
                            pin.is_reset = true;
                            pin.exit_side = PinExitSide::SOUTH;
                        } else if (r_it->second == PinRole::PRESET) {
                            pin.exit_side = PinExitSide::NORTH;
                        } else if (r_it->second == PinRole::SELECT) {
                            pin.exit_side = PinExitSide::SOUTH;
                        }
                    }
                }
            } else if (submod != nullptr) {
                // Direction from RTL submodule or blackbox stub in design
                RTLIL::Wire* sub_wire = submod->wire(conn.first);
                if (sub_wire) {
                    if (sub_wire->port_output) {
                        pin.direction = PinDirection::OUTPUT;
                        pin.exit_side = PinExitSide::EAST;
                    } else if (sub_wire->port_input) {
                        pin.direction = PinDirection::INPUT;
                        pin.exit_side = PinExitSide::WEST;
                    } else {
                        pin.direction = PinDirection::INOUT;
                        pin.exit_side = PinExitSide::EAST;
                    }
                } else {
                    pin.direction = PinDirection::INPUT;
                    pin.exit_side = PinExitSide::WEST;
                }
            } else if (cell_type.starts_with("$_") || cell_type.starts_with("$")) {
                // Built-in RTLIL primitive ports
                auto sym_class = SymbolClassifier::classify(cell_type);
                auto r_it = sym_class.pin_roles.find(pin_name);
                if (r_it != sym_class.pin_roles.end()) {
                    if (r_it->second == PinRole::OUTPUT || r_it->second == PinRole::Q || r_it->second == PinRole::QN) {
                        pin.direction = PinDirection::OUTPUT;
                        pin.exit_side = PinExitSide::EAST;
                    } else {
                        pin.direction = PinDirection::INPUT;
                        pin.exit_side = PinExitSide::WEST;
                    }
                    if (r_it->second == PinRole::CLOCK) {
                        pin.is_clock = true;
                        pin.exit_side = PinExitSide::SOUTH;
                    } else if (r_it->second == PinRole::RESET) {
                        pin.is_reset = true;
                        pin.exit_side = PinExitSide::SOUTH;
                    } else if (r_it->second == PinRole::PRESET) {
                        pin.exit_side = PinExitSide::NORTH;
                    }
                } else {
                    pin.direction = PinDirection::INPUT;
                    pin.exit_side = PinExitSide::WEST;
                }
                for (const auto& bp : sym_class.bubble_pins) {
                    if (bp == pin_name) {
                        pin.is_inverted = true;
                        break;
                    }
                }
            } else {
                // Completely unresolved ASIC cell without Liberty
                pin.direction = PinDirection::INPUT;
                pin.exit_side = PinExitSide::WEST;
            }

            scell.pins.push_back(pin);
        }

        target.cells[cell_id] = scell;
    }
}

void RTLILAdapter::extract_assigns(RTLIL::Module* module, SchematicModule& target) {
    int assign_idx = 0;
    for (const auto& conn : module->connections()) {
        if (conn.first.size() == conn.second.size() && conn.first.size() > 1) {
            for (int i = 0; i < conn.first.size(); ++i) {
                std::string lhs_net = sigspec_to_net_name(conn.first[i]);
                std::string rhs_net = sigspec_to_net_name(conn.second[i]);

                if (lhs_net.empty() || rhs_net.empty() || lhs_net == rhs_net) continue;

                std::string buf_id = "$assign$" + std::to_string(++assign_idx);
                SchematicCell bcell;
                bcell.id = buf_id;
                bcell.name = buf_id;
                bcell.cell_type = "$_BUF_";
                bcell.type = NodeType::GATE;

                SchematicPin in_pin;
                in_pin.name = "A";
                in_pin.owner_node_id = buf_id;
                in_pin.direction = PinDirection::INPUT;
                in_pin.exit_side = PinExitSide::WEST;
                in_pin.net_name = rhs_net;
                bcell.pins.push_back(in_pin);

                SchematicPin out_pin;
                out_pin.name = "Y";
                out_pin.owner_node_id = buf_id;
                out_pin.direction = PinDirection::OUTPUT;
                out_pin.exit_side = PinExitSide::EAST;
                out_pin.net_name = lhs_net;
                bcell.pins.push_back(out_pin);

                target.cells[buf_id] = bcell;
            }
        } else {
            std::string lhs_net = sigspec_to_net_name(conn.first);
            std::string rhs_net = sigspec_to_net_name(conn.second);

            if (lhs_net.empty() || rhs_net.empty() || lhs_net == rhs_net) continue;

            // Represent wire assignment as a buffer gate if not directly unified
            std::string buf_id = "$assign$" + std::to_string(++assign_idx);
            SchematicCell bcell;
            bcell.id = buf_id;
            bcell.name = buf_id;
            bcell.cell_type = "$_BUF_";
            bcell.type = NodeType::GATE;

            SchematicPin in_pin;
            in_pin.name = "A";
            in_pin.owner_node_id = buf_id;
            in_pin.direction = PinDirection::INPUT;
            in_pin.exit_side = PinExitSide::WEST;
            in_pin.net_name = rhs_net;
            bcell.pins.push_back(in_pin);

            SchematicPin out_pin;
            out_pin.name = "Y";
            out_pin.owner_node_id = buf_id;
            out_pin.direction = PinDirection::OUTPUT;
            out_pin.exit_side = PinExitSide::EAST;
            out_pin.net_name = lhs_net;
            bcell.pins.push_back(out_pin);

            target.cells[buf_id] = bcell;
        }
    }
}

void RTLILAdapter::reconcile_nets(SchematicModule& target) {
    // 1. Register Primary Input Ports as Drivers
    for (const auto& [pname, port] : target.ports) {
        if (port.direction == PinDirection::INPUT || port.direction == PinDirection::INOUT) {
            SchematicNet& net = target.nets[port.net_name];
            net.name = port.net_name;
            net.driver_pin = "PORT:" + pname;
            net.is_clock = port.is_clock;
            net.is_reset = port.is_reset;
        } else if (port.direction == PinDirection::OUTPUT) {
            SchematicNet& net = target.nets[port.net_name];
            net.name = port.net_name;
            net.sink_pins.push_back("PORT:" + pname);
        }
    }

    // 2. Register Cell Pins as Drivers and Sinks
    for (const auto& [cid, cell] : target.cells) {
        for (const auto& pin : cell.pins) {
            if (pin.net_name.empty()) continue;

            SchematicNet& net = target.nets[pin.net_name];
            net.name = pin.net_name;
            std::string pin_key = cid + ":" + pin.name;

            if (pin.direction == PinDirection::OUTPUT) {
                net.driver_pin = pin_key;
            } else {
                net.sink_pins.push_back(pin_key);
            }

            if (pin.is_clock) net.is_clock = true;
            if (pin.is_reset) net.is_reset = true;
        }
    }
}

} // namespace vajra::gui
