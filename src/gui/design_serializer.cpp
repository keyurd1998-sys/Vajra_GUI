#include "vajra/gui/design_serializer.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

namespace vajra::gui {

namespace {

std::string encode_str(const std::string& s) {
    if (s.empty()) return "%EMPTY%";
    std::string out;
    for (char c : s) {
        if (c == ' ') out += "%20";
        else if (c == '\t') out += "%09";
        else if (c == '\n') out += "%0A";
        else if (c == '%') out += "%25";
        else out += c;
    }
    return out;
}

std::string decode_str(const std::string& s) {
    if (s == "%EMPTY%") return "";
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            std::string hex = s.substr(i + 1, 2);
            if (hex == "20") { out += ' '; i += 2; continue; }
            if (hex == "09") { out += '\t'; i += 2; continue; }
            if (hex == "0A") { out += '\n'; i += 2; continue; }
            if (hex == "25") { out += '%'; i += 2; continue; }
        }
        out += s[i];
    }
    return out;
}

} // anonymous namespace

bool DesignSerializer::save_to_file(const SchematicDesign& design, const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << "VAJRA_SCHEMATIC_DESIGN_V1\n";
    ofs << "TOP " << encode_str(design.top_module) << "\n";
    ofs << "NUM_MODULES " << design.modules.size() << "\n";

    for (const auto& [mname, mod] : design.modules) {
        ofs << "MODULE " << encode_str(mod.name) << "\n";

        // Ports
        ofs << "NUM_PORTS " << mod.ports.size() << "\n";
        for (const auto& [pname, p] : mod.ports) {
            ofs << "PORT " << encode_str(p.name) << " "
                << static_cast<int>(p.direction) << " "
                << (p.is_clock ? 1 : 0) << " "
                << (p.is_reset ? 1 : 0) << " "
                << encode_str(p.net_name) << "\n";
        }

        // Cells
        ofs << "NUM_CELLS " << mod.cells.size() << "\n";
        for (const auto& [cid, cell] : mod.cells) {
            ofs << "CELL " << encode_str(cell.id) << " "
                << encode_str(cell.name) << " "
                << encode_str(cell.cell_type) << " "
                << static_cast<int>(cell.type) << " "
                << (cell.is_submodule ? 1 : 0) << " "
                << encode_str(cell.submodule_target) << "\n";

            // Pins
            ofs << "NUM_PINS " << cell.pins.size() << "\n";
            for (const auto& pin : cell.pins) {
                ofs << "PIN " << encode_str(pin.name) << " "
                    << static_cast<int>(pin.direction) << " "
                    << static_cast<int>(pin.exit_side) << " "
                    << encode_str(pin.net_name) << " "
                    << (pin.is_clock ? 1 : 0) << " "
                    << (pin.is_reset ? 1 : 0) << " "
                    << (pin.is_inverted ? 1 : 0) << " "
                    << pin.bit_index << "\n";
            }

            // Attributes
            ofs << "NUM_ATTRS " << cell.attributes.size() << "\n";
            for (const auto& [ak, av] : cell.attributes) {
                ofs << "ATTR " << encode_str(ak) << " " << encode_str(av) << "\n";
            }
        }

        // Nets
        ofs << "NUM_NETS " << mod.nets.size() << "\n";
        for (const auto& [nname, net] : mod.nets) {
            ofs << "NET " << encode_str(net.name) << " "
                << encode_str(net.driver_pin) << " "
                << (net.is_clock ? 1 : 0) << " "
                << (net.is_reset ? 1 : 0) << " "
                << (net.is_bus ? 1 : 0) << " "
                << net.width << "\n";

            ofs << "NUM_SINKS " << net.sink_pins.size() << "\n";
            for (const auto& sink : net.sink_pins) {
                ofs << "SINK " << encode_str(sink) << "\n";
            }
        }

        ofs << "END_MODULE\n";
    }

    return ofs.good();
}

bool DesignSerializer::load_from_file(const std::string& filepath, SchematicDesign& design) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    std::string magic;
    if (!(ifs >> magic) || magic != "VAJRA_SCHEMATIC_DESIGN_V1") {
        return false;
    }

    design.modules.clear();

    std::string tag;
    while (ifs >> tag) {
        if (tag == "TOP") {
            std::string top_val;
            ifs >> top_val;
            design.top_module = decode_str(top_val);
        } else if (tag == "NUM_MODULES") {
            size_t n_mod = 0;
            ifs >> n_mod;
        } else if (tag == "MODULE") {
            std::string mname_raw;
            ifs >> mname_raw;
            SchematicModule mod;
            mod.name = decode_str(mname_raw);

            std::string subtag;
            while (ifs >> subtag && subtag != "END_MODULE") {
                if (subtag == "NUM_PORTS") {
                    size_t n_ports = 0;
                    ifs >> n_ports;
                    for (size_t i = 0; i < n_ports; ++i) {
                        std::string ptag, pname, pnet;
                        int pdir = 0, is_clk = 0, is_rst = 0;
                        ifs >> ptag >> pname >> pdir >> is_clk >> is_rst >> pnet;
                        SchematicPin p;
                        p.name = decode_str(pname);
                        p.direction = static_cast<PinDirection>(pdir);
                        p.is_clock = (is_clk != 0);
                        p.is_reset = (is_rst != 0);
                        p.net_name = decode_str(pnet);
                        p.owner_node_id = "PORT:" + p.name;
                        mod.ports[p.name] = p;
                    }
                } else if (subtag == "NUM_CELLS") {
                    size_t n_cells = 0;
                    ifs >> n_cells;
                    for (size_t i = 0; i < n_cells; ++i) {
                        std::string ctag, cid, cname, ctype, sub_target;
                        int cnode_type = 0, is_sub = 0;
                        ifs >> ctag >> cid >> cname >> ctype >> cnode_type >> is_sub >> sub_target;
                        SchematicCell cell;
                        cell.id = decode_str(cid);
                        cell.name = decode_str(cname);
                        cell.cell_type = decode_str(ctype);
                        cell.type = static_cast<NodeType>(cnode_type);
                        cell.is_submodule = (is_sub != 0);
                        cell.submodule_target = decode_str(sub_target);

                        std::string psub;
                        ifs >> psub; // NUM_PINS
                        size_t n_pins = 0;
                        ifs >> n_pins;
                        for (size_t pi = 0; pi < n_pins; ++pi) {
                            std::string pintag, pin_name, net_name;
                            int pdir = 0, pside = 0, is_clk = 0, is_rst = 0, is_inv = 0, bit_idx = -1;
                            ifs >> pintag >> pin_name >> pdir >> pside >> net_name >> is_clk >> is_rst >> is_inv >> bit_idx;
                            SchematicPin pin;
                            pin.name = decode_str(pin_name);
                            pin.owner_node_id = cell.id;
                            pin.direction = static_cast<PinDirection>(pdir);
                            pin.exit_side = static_cast<PinExitSide>(pside);
                            pin.net_name = decode_str(net_name);
                            pin.is_clock = (is_clk != 0);
                            pin.is_reset = (is_rst != 0);
                            pin.is_inverted = (is_inv != 0);
                            pin.bit_index = bit_idx;
                            cell.pins.push_back(pin);
                        }

                        std::string asub;
                        ifs >> asub; // NUM_ATTRS
                        size_t n_attrs = 0;
                        ifs >> n_attrs;
                        for (size_t ai = 0; ai < n_attrs; ++ai) {
                            std::string atag, ak, av;
                            ifs >> atag >> ak >> av;
                            cell.attributes[decode_str(ak)] = decode_str(av);
                        }

                        mod.cells[cell.id] = cell;
                    }
                } else if (subtag == "NUM_NETS") {
                    size_t n_nets = 0;
                    ifs >> n_nets;
                    for (size_t i = 0; i < n_nets; ++i) {
                        std::string ntag, nname, drv;
                        int is_clk = 0, is_rst = 0, is_bus = 0, width = 1;
                        ifs >> ntag >> nname >> drv >> is_clk >> is_rst >> is_bus >> width;
                        SchematicNet net;
                        net.name = decode_str(nname);
                        net.driver_pin = decode_str(drv);
                        net.is_clock = (is_clk != 0);
                        net.is_reset = (is_rst != 0);
                        net.is_bus = (is_bus != 0);
                        net.width = width;

                        std::string ssub;
                        ifs >> ssub; // NUM_SINKS
                        size_t n_sinks = 0;
                        ifs >> n_sinks;
                        for (size_t si = 0; si < n_sinks; ++si) {
                            std::string stag, sname;
                            ifs >> stag >> sname;
                            net.sink_pins.push_back(decode_str(sname));
                        }

                        mod.nets[net.name] = net;
                    }
                }
            }

            design.modules[mod.name] = mod;
        }
    }

    return true;
}

} // namespace vajra::gui
