#pragma once

#include "vajra/gui/models.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace vajra::gui {

struct PlacementEdge {
    std::string src;        // Source node ID
    std::string dst;        // Destination node ID
    std::string net_name;   // Net identifier
    std::string src_pin;
    std::string dst_pin;
    bool is_feedback{false};
};

struct PlacementNode {
    std::string id;
    std::string kind;       // "PRIMARY_INPUT", "PRIMARY_OUTPUT", "GATE", "DFF", "MODULE"
    std::string name;
    std::string cell_type;
    double width{80.0};
    double height{50.0};
    int rank{0};
    int order{0};
    double x{0.0};
    double y{0.0};
    std::vector<std::string> preds;
    std::vector<std::string> succs;

    bool is_sequential() const {
        return kind == "DFF" || kind == "SDFF" || kind == "LATCH";
    }

    bool is_port() const {
        return kind == "PRIMARY_INPUT" || kind == "PRIMARY_OUTPUT";
    }
};

class PlacementGraph {
public:
    std::unordered_map<std::string, PlacementNode> nodes;
    std::vector<PlacementEdge> edges;
    std::vector<std::vector<std::string>> ranks;

    void add_node(PlacementNode node) {
        nodes[node.id] = std::move(node);
    }

    void add_edge(PlacementEdge edge) {
        edges.push_back(edge);
        if (!edge.is_feedback) {
            auto& src_node = nodes[edge.src];
            if (std::find(src_node.succs.begin(), src_node.succs.end(), edge.dst) == src_node.succs.end()) {
                src_node.succs.push_back(edge.dst);
            }
            auto& dst_node = nodes[edge.dst];
            if (std::find(dst_node.preds.begin(), dst_node.preds.end(), edge.src) == dst_node.preds.end()) {
                dst_node.preds.push_back(edge.src);
            }
        }
    }

    PlacementNode* get_node(const std::string& node_id) {
        auto it = nodes.find(node_id);
        return it != nodes.end() ? &it->second : nullptr;
    }

    const PlacementNode* get_node(const std::string& node_id) const {
        auto it = nodes.find(node_id);
        return it != nodes.end() ? &it->second : nullptr;
    }
};

} // namespace vajra::gui
