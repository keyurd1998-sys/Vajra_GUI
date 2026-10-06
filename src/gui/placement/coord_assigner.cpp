#include "vajra/gui/placement/coord_assigner.hpp"

#include <algorithm>

namespace vajra::gui {

Rect CoordinateAssigner::assign_coordinates(PlacementGraph& graph,
                                           double col_spacing,
                                           double row_spacing,
                                           bool center_align) {
    if (graph.ranks.empty() || graph.nodes.empty()) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    // 1. Column dimensions
    std::vector<double> col_widths;
    std::vector<double> col_heights;
    double max_overall_height = 0.0;

    for (const auto& rank_nodes : graph.ranks) {
        double max_w = 60.0;
        double total_h = 0.0;
        for (const auto& nid : rank_nodes) {
            const auto* node = graph.get_node(nid);
            if (!node) continue;
            max_w = std::max(max_w, node->width);
            total_h += node->height + row_spacing;
        }
        col_widths.push_back(max_w);
        col_heights.push_back(std::max(total_h, 60.0));
        max_overall_height = std::max(max_overall_height, total_h);
    }

    // 2. Compute X positions monotonically with dynamic spacing for dense routing channels
    std::vector<double> col_x_offsets;
    double curr_x = 50.0;
    for (size_t r = 0; r < col_widths.size(); ++r) {
        col_x_offsets.push_back(curr_x);
        double dynamic_spacing = col_spacing;
        if (r < graph.ranks.size()) {
            size_t rank_size = graph.ranks[r].size();
            if (rank_size > 8) {
                dynamic_spacing = std::max(col_spacing, std::min(300.0, col_spacing + (rank_size - 8) * 6.0));
            }
        }
        curr_x += col_widths[r] + dynamic_spacing;
    }

    // 3. Assign (X, Y) to nodes with balanced, non-congested vertical distribution
    for (size_t r = 0; r < graph.ranks.size(); ++r) {
        double rank_x = col_x_offsets[r];
        const auto& rank_nodes = graph.ranks[r];
        if (rank_nodes.empty()) continue;

        double total_node_h = 0.0;
        for (const auto& nid : rank_nodes) {
            const auto* node = graph.get_node(nid);
            if (node) total_node_h += node->height;
        }

        // Even distribution: expand spacing up to 2.8x row_spacing for shorter columns
        // so placement is distributed comfortably across the canvas without congestion
        double effective_spacing = row_spacing;
        if (rank_nodes.size() > 1) {
            double span_spacing = (max_overall_height - total_node_h) / static_cast<double>(rank_nodes.size() - 1);
            effective_spacing = std::max(row_spacing, std::min(row_spacing * 2.8, span_spacing));
        }

        double rank_actual_h = total_node_h + (rank_nodes.size() > 1 ? (rank_nodes.size() - 1) * effective_spacing : 0.0);
        double start_y = center_align ? (50.0 + (max_overall_height - rank_actual_h) / 2.0) : 50.0;
        if (start_y < 50.0) start_y = 50.0;

        double curr_y = start_y;
        for (const auto& nid : rank_nodes) {
            auto* node = graph.get_node(nid);
            if (!node) continue;
            node->x = rank_x;
            node->y = curr_y;
            curr_y += node->height + effective_spacing;
        }
    }

    // 4. Compute overall bounding box
    double min_x = 1e9, min_y = 1e9;
    double max_x = -1e9, max_y = -1e9;

    for (const auto& [nid, node] : graph.nodes) {
        min_x = std::min(min_x, node.x);
        min_y = std::min(min_y, node.y);
        max_x = std::max(max_x, node.x + node.width);
        max_y = std::max(max_y, node.y + node.height);
    }

    if (min_x > max_x) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    return {min_x, min_y, max_x - min_x, max_y - min_y};
}

} // namespace vajra::gui
