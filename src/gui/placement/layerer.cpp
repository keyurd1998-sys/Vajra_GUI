#include "vajra/gui/placement/layerer.hpp"

#include <queue>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace vajra::gui {

int LayerAssigner::assign_layers(PlacementGraph& graph) {
    if (graph.nodes.empty()) {
        graph.ranks.clear();
        return 0;
    }

    // 1. Compute in-degree in the DAG
    std::unordered_map<std::string, int> in_degree;
    for (const auto& [nid, node] : graph.nodes) {
        in_degree[nid] = static_cast<int>(node.preds.size());
    }

    // 2. Queue all source nodes (in-degree 0)
    std::queue<std::string> queue;
    for (const auto& [nid, deg] : in_degree) {
        if (deg == 0) {
            graph.nodes[nid].rank = 0;
            queue.push(nid);
        }
    }

    int max_rank = 0;

    // 3. Process nodes in topological order (Longest-Path / ASAP leveling)
    while (!queue.empty()) {
        std::string u_id = queue.front();
        queue.pop();
        int u_rank = graph.nodes[u_id].rank;

        for (const std::string& v_id : graph.nodes[u_id].succs) {
            auto& v_node = graph.nodes[v_id];
            if (u_rank + 1 > v_node.rank) {
                v_node.rank = u_rank + 1;
                if (v_node.rank > max_rank) {
                    max_rank = v_node.rank;
                }
            }

            in_degree[v_id]--;
            if (in_degree[v_id] == 0) {
                queue.push(v_id);
            }
        }
    }

    // 4. Handle any nodes with unresolved dependencies
    for (const auto& [nid, deg] : in_degree) {
        if (deg > 0 && graph.nodes[nid].rank == 0) {
            graph.nodes[nid].rank = 0;
        }
    }

    // 5. Shift primary outputs to the rightmost rank or just beyond their highest predecessor
    for (auto& [nid, node] : graph.nodes) {
        if (node.kind == "PRIMARY_OUTPUT") {
            if (!node.preds.empty()) {
                int highest_pred = 0;
                for (const auto& p : node.preds) {
                    highest_pred = std::max(highest_pred, graph.nodes[p].rank);
                }
                node.rank = highest_pred + 1;
                if (node.rank > max_rank) {
                    max_rank = node.rank;
                }
            } else {
                node.rank = max_rank;
            }
        }
    }

    // 6. Group nodes into initial ranks
    std::vector<std::vector<std::string>> initial_ranks(max_rank + 1);
    for (const auto& [nid, node] : graph.nodes) {
        initial_ranks[node.rank].push_back(nid);
    }

    // 7. Adaptive multi-column rank folding to widen schematic and balance aspect ratio
    // In topological leveling, all nodes in the same rank form an independent set (no edges
    // exist between any two nodes in the same rank). Subdividing tall ranks into sequential
    // sub-ranks strictly preserves topological monotonicity (zero backward or feedback edges).
    size_t total_nodes = graph.nodes.size();
    // Balanced aspect-ratio leveling: target width-to-height ratio ~ 1.5 - 1.8
    // Prevent over-folding while distributing nodes comfortably across canvas columns.
    int target_col_nodes = static_cast<int>(std::round(1.05 * std::sqrt(total_nodes)));
    int max_col_nodes = std::max(8, std::min(16, target_col_nodes));
    const int max_port_nodes = 20;

    std::vector<std::vector<std::string>> folded_ranks;
    folded_ranks.reserve(initial_ranks.size() * 2);

    for (size_t r = 0; r < initial_ranks.size(); ++r) {
        auto& rank_nodes = initial_ranks[r];
        if (rank_nodes.empty()) {
            continue;
        }

        // Determine if this rank consists solely of ports
        bool all_ports = true;
        for (const auto& nid : rank_nodes) {
            if (!graph.nodes[nid].is_port()) {
                all_ports = false;
                break;
            }
        }

        int limit = all_ports ? max_port_nodes : max_col_nodes;

        if (static_cast<int>(rank_nodes.size()) <= limit) {
            folded_ranks.push_back(std::move(rank_nodes));
        } else {
            // Sort nodes deterministically:
            // 1. Group by node kind: DFFs / Latches first, then Gates / Modules, then Ports
            // 2. Secondary sort: Alphanumeric by node name / bus index
            std::sort(rank_nodes.begin(), rank_nodes.end(), [&](const std::string& a, const std::string& b) {
                const auto& na = graph.nodes[a];
                const auto& nb = graph.nodes[b];
                int kind_a = na.is_sequential() ? 0 : (na.kind == "MODULE" ? 1 : (na.is_port() ? 3 : 2));
                int kind_b = nb.is_sequential() ? 0 : (nb.kind == "MODULE" ? 1 : (nb.is_port() ? 3 : 2));
                if (kind_a != kind_b) return kind_a < kind_b;
                return na.name < nb.name;
            });

            int num_splits = (static_cast<int>(rank_nodes.size()) + limit - 1) / limit;
            size_t total_in_rank = rank_nodes.size();
            for (int s = 0; s < num_splits; ++s) {
                size_t start_idx = s * total_in_rank / num_splits;
                size_t end_idx = (s + 1) * total_in_rank / num_splits;
                if (start_idx < end_idx && start_idx < total_in_rank) {
                    folded_ranks.emplace_back(rank_nodes.begin() + start_idx,
                                             rank_nodes.begin() + std::min(end_idx, total_in_rank));
                }
            }
        }
    }

    if (folded_ranks.empty()) {
        folded_ranks.push_back({});
    }

    // 8. Re-assign final rank indices to nodes
    for (size_t r = 0; r < folded_ranks.size(); ++r) {
        for (const auto& nid : folded_ranks[r]) {
            graph.nodes[nid].rank = static_cast<int>(r);
        }
    }

    graph.ranks = std::move(folded_ranks);
    return static_cast<int>(graph.ranks.size() - 1);
}

} // namespace vajra::gui
