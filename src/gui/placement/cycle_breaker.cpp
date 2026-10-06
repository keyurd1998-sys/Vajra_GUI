#include "vajra/gui/placement/cycle_breaker.hpp"

#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace vajra::gui {

namespace {

std::pair<std::string, std::string> parse_ref(const std::string& ref) {
    if (ref.rfind("PORT:", 0) == 0) {
        return {"PORT", ref.substr(5)};
    }
    if (ref.rfind("port:", 0) == 0) {
        return {"PORT", ref.substr(5)};
    }
    auto pos = ref.rfind(':');
    if (pos == std::string::npos) return {ref, ref};
    return {ref.substr(0, pos), ref.substr(pos + 1)};
}

} // anonymous namespace

PlacementGraph CycleBreaker::build_dag(const SchematicModule& module, bool decouple_dff) {
    PlacementGraph graph;

    // 1. Primary input & output ports (sorted deterministically)
    std::vector<std::string> port_names;
    for (const auto& [name, _] : module.ports) {
        port_names.push_back(name);
    }
    std::sort(port_names.begin(), port_names.end());

    for (const auto& name : port_names) {
        const auto& port = module.ports.at(name);
        if (port.name.find('[') == std::string::npos && module.ports.find(port.name + "[0]") != module.ports.end()) {
            continue;
        }

        bool is_bus = (port.name.find('[') != std::string::npos);
        PlacementNode node;
        node.name = port.name;
        node.id = "port:" + port.name;
        if (port.direction == PinDirection::INPUT || port.direction == PinDirection::INOUT) {
            node.kind = "PRIMARY_INPUT";
            node.width = is_bus ? 110.0 : 75.0;
            node.height = 28.0;
        } else {
            node.kind = "PRIMARY_OUTPUT";
            node.width = is_bus ? 110.0 : 75.0;
            node.height = 28.0;
        }
        graph.add_node(std::move(node));
    }

    // 2. Cell nodes (sorted deterministically)
    std::vector<std::string> cell_names;
    for (const auto& [name, _] : module.cells) {
        cell_names.push_back(name);
    }
    std::sort(cell_names.begin(), cell_names.end());

    for (const auto& cell_id : cell_names) {
        const auto& cell = module.cells.at(cell_id);
        PlacementNode node;
        node.id = cell.name;
        node.name = cell.name;
        node.cell_type = cell.cell_type;

        if (cell.is_submodule || cell.type == NodeType::MODULE) {
            node.kind = "MODULE";
            size_t max_name_len = cell.name.size() + cell.cell_type.size() + 8;
            node.width = std::max(160.0, static_cast<double>(max_name_len) * 7.5);
            size_t in_count = 0, out_count = 0;
            for (const auto& pin : cell.pins) {
                if (pin.direction == PinDirection::INPUT) in_count++;
                else out_count++;
            }
            size_t max_pins = std::max({in_count, out_count, size_t{1}});
            node.height = std::max(80.0, 24.0 + 18.0 + max_pins * 20.0);
        } else if (cell.type == NodeType::DFF || cell.type == NodeType::LATCH) {
            bool is_sdff = (cell.attributes.count("symbol_gate_type") && cell.attributes.at("symbol_gate_type") == "SDFF");
            node.kind = is_sdff ? "SDFF" : (cell.type == NodeType::LATCH ? "LATCH" : "DFF");
            node.width = is_sdff ? 110.0 : 100.0;
            node.height = 70.0;
        } else {
            node.kind = "GATE";
            if (cell.cell_type == "$_NOT_" || cell.cell_type == "$_BUF_") {
                node.width = 60.0;
                node.height = 40.0;
            } else {
                node.width = 80.0;
                node.height = 50.0;
            }
        }
        graph.add_node(std::move(node));
    }

    // 3. Edges from nets (sorted deterministically)
    struct RawEdge {
        std::string src;
        std::string dst;
        std::string net_name;
        std::string src_pin;
        std::string dst_pin;
    };
    std::vector<RawEdge> raw_edges;

    std::vector<std::string> net_names;
    for (const auto& [net_name, _] : module.nets) {
        net_names.push_back(net_name);
    }
    std::sort(net_names.begin(), net_names.end());

    for (const auto& net_name : net_names) {
        const auto& net = module.nets.at(net_name);
        if (net.driver_pin.empty()) continue;
        auto [drv_owner, drv_pin] = parse_ref(net.driver_pin);
        std::string src_id = (drv_owner == "PORT") ? ("port:" + drv_pin) : drv_owner;

        if (!graph.get_node(src_id)) continue;

        for (const auto& sink_ref : net.sink_pins) {
            auto [snk_owner, snk_pin] = parse_ref(sink_ref);
            std::string dst_id = (snk_owner == "PORT") ? ("port:" + snk_pin) : snk_owner;

            if (!graph.get_node(dst_id)) continue;
            if (src_id == dst_id) continue; // Self-loop

            raw_edges.push_back({src_id, dst_id, net_name, drv_pin, snk_pin});
        }
    }

    // 4. Score nodes for DFS exploration priority:
    // Upstream control / internal logic should be explored before downstream output buffers
    std::unordered_map<std::string, int> pi_connections;
    std::unordered_map<std::string, int> po_connections;
    for (const auto& e : raw_edges) {
        if (e.src.rfind("port:", 0) == 0) {
            pi_connections[e.dst]++;
        }
        if (e.dst.rfind("port:", 0) == 0) {
            po_connections[e.src]++;
        }
    }

    // Compute priority: higher priority visited first
    // Modules driving primary outputs or buffer sinks (like FIFOs) should be explored as sinks
    auto get_priority = [&](const std::string& nid) -> int {
        int pi = pi_connections[nid];
        int po = po_connections[nid];
        bool is_buffer_sink = (nid.find("FIFO") != std::string::npos || nid.find("fifo") != std::string::npos ||
                               nid.find("buf") != std::string::npos || nid.find("BUF") != std::string::npos);
        return (pi * 20) - (is_buffer_sink ? 100 : 0) - (po * 2);
    };

    std::unordered_map<std::string, std::vector<size_t>> adj;
    for (size_t i = 0; i < raw_edges.size(); ++i) {
        adj[raw_edges[i].src].push_back(i);
    }

    // Sort edges in adj[u] by descending destination priority, then alphabetically
    for (auto& [u, edge_indices] : adj) {
        std::sort(edge_indices.begin(), edge_indices.end(), [&](size_t a, size_t b) {
            const auto& ea = raw_edges[a];
            const auto& eb = raw_edges[b];
            int pri_a = get_priority(ea.dst);
            int pri_b = get_priority(eb.dst);
            if (pri_a != pri_b) return pri_a > pri_b;
            if (ea.dst != eb.dst) return ea.dst < eb.dst;
            return ea.net_name < eb.net_name;
        });
    }

    std::unordered_map<std::string, int> color;
    for (const auto& [nid, _] : graph.nodes) {
        color[nid] = 0;
    }

    std::vector<bool> is_feedback(raw_edges.size(), false);

    // If decouple_dff is enabled, decouple sequential feedback loops into DFF inputs
    if (decouple_dff) {
        for (size_t i = 0; i < raw_edges.size(); ++i) {
            const auto& e = raw_edges[i];
            const auto* dst_n = graph.get_node(e.dst);
            const auto* src_n = graph.get_node(e.src);
            if (dst_n && dst_n->is_sequential() && src_n && !src_n->is_port()) {
                std::string lp = e.dst_pin;
                std::transform(lp.begin(), lp.end(), lp.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (lp == "d" || lp == "data" || lp.rfind("d", 0) == 0 || lp == "si") {
                    is_feedback[i] = true;
                }
            }
        }
    }

    // Iterative DFS to safely detect cycles without risking stack overflow on large netlists
    auto dfs = [&](const std::string& start_node) {
        std::vector<std::pair<std::string, size_t>> stack;
        color[start_node] = 1; // Gray
        stack.push_back({start_node, 0});

        while (!stack.empty()) {
            auto& [u, next_idx] = stack.back();
            auto it_adj = adj.find(u);
            if (it_adj == adj.end() || next_idx >= it_adj->second.size()) {
                color[u] = 2; // Black
                stack.pop_back();
                continue;
            }

            size_t edge_idx = it_adj->second[next_idx++];
            if (is_feedback[edge_idx]) continue;

            const std::string& v = raw_edges[edge_idx].dst;
            if (color[v] == 1) {
                // Back-edge detected!
                is_feedback[edge_idx] = true;
            } else if (color[v] == 0) {
                color[v] = 1; // Gray
                stack.push_back({v, 0});
            }
        }
    };

    // First visit primary inputs in prioritized order:
    // Inputs driving control logic visited before inputs driving datapath/buffers
    std::vector<std::string> pi_nodes;
    for (const auto& [nid, node] : graph.nodes) {
        if (node.kind == "PRIMARY_INPUT") {
            pi_nodes.push_back(nid);
        }
    }
    std::sort(pi_nodes.begin(), pi_nodes.end(), [&](const std::string& a, const std::string& b) {
        int max_pri_a = -1000;
        for (size_t eidx : adj[a]) max_pri_a = std::max(max_pri_a, get_priority(raw_edges[eidx].dst));
        int max_pri_b = -1000;
        for (size_t eidx : adj[b]) max_pri_b = std::max(max_pri_b, get_priority(raw_edges[eidx].dst));
        if (max_pri_a != max_pri_b) return max_pri_a > max_pri_b;
        return a < b;
    });

    for (const auto& nid : pi_nodes) {
        if (color[nid] == 0) {
            dfs(nid);
        }
    }

    // Then visit remaining nodes sorted by priority
    std::vector<std::string> remaining_nodes;
    for (const auto& [nid, node] : graph.nodes) {
        if (color[nid] == 0) {
            remaining_nodes.push_back(nid);
        }
    }
    std::sort(remaining_nodes.begin(), remaining_nodes.end(), [&](const std::string& a, const std::string& b) {
        int pri_a = get_priority(a);
        int pri_b = get_priority(b);
        if (pri_a != pri_b) return pri_a > pri_b;
        return a < b;
    });

    for (const auto& nid : remaining_nodes) {
        if (color[nid] == 0) {
            dfs(nid);
        }
    }

    // 5. Add edges to graph
    for (size_t i = 0; i < raw_edges.size(); ++i) {
        PlacementEdge pe;
        pe.src = raw_edges[i].src;
        pe.dst = raw_edges[i].dst;
        pe.net_name = raw_edges[i].net_name;
        pe.src_pin = raw_edges[i].src_pin;
        pe.dst_pin = raw_edges[i].dst_pin;
        pe.is_feedback = is_feedback[i];
        graph.add_edge(std::move(pe));
    }

    return graph;
}

} // namespace vajra::gui
