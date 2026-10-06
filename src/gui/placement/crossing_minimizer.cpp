#include "vajra/gui/placement/crossing_minimizer.hpp"

#include <unordered_map>
#include <algorithm>
#include <tuple>

namespace vajra::gui {

void CrossingMinimizer::minimize(PlacementGraph& graph, int iterations) {
    if (graph.ranks.empty()) return;

    // 1. Initial ordering assignment
    for (size_t r = 0; r < graph.ranks.size(); ++r) {
        for (size_t i = 0; i < graph.ranks[r].size(); ++i) {
            graph.nodes[graph.ranks[r][i]].order = static_cast<int>(i);
        }
    }

    std::unordered_map<std::string, double> pos_map;
    for (const auto& [nid, node] : graph.nodes) {
        pos_map[nid] = static_cast<double>(node.order);
    }

    // 2. Alternating sweeps
    for (int iter = 0; iter < iterations; ++iter) {
        // Forward sweep: rank 1 up to max_rank
        for (size_t r = 1; r < graph.ranks.size(); ++r) {
            std::vector<std::tuple<double, double, std::string>> barycenters;
            for (const std::string& nid : graph.ranks[r]) {
                const auto& node = graph.nodes[nid];
                double sum_pos = 0.0;
                int count = 0;
                for (const auto& p : node.preds) {
                    if (pos_map.find(p) != pos_map.end()) {
                        sum_pos += pos_map[p];
                        count++;
                    }
                }
                double bc = (count > 0) ? (sum_pos / count) : pos_map[nid];
                barycenters.emplace_back(bc, pos_map[nid], nid);
            }

            // Stable sort by barycenter, then old order
            std::sort(barycenters.begin(), barycenters.end(),
                      [](const auto& a, const auto& b) {
                          if (std::abs(std::get<0>(a) - std::get<0>(b)) > 1e-5) {
                              return std::get<0>(a) < std::get<0>(b);
                          }
                          return std::get<1>(a) < std::get<1>(b);
                      });

            for (size_t i = 0; i < barycenters.size(); ++i) {
                const std::string& nid = std::get<2>(barycenters[i]);
                graph.ranks[r][i] = nid;
                graph.nodes[nid].order = static_cast<int>(i);
                pos_map[nid] = static_cast<double>(i);
            }
        }

        // Backward sweep: max_rank - 1 down to 0
        if (graph.ranks.size() >= 2) {
            for (int r = static_cast<int>(graph.ranks.size()) - 2; r >= 0; --r) {
                std::vector<std::tuple<double, double, std::string>> barycenters;
                for (const std::string& nid : graph.ranks[r]) {
                    const auto& node = graph.nodes[nid];
                    double sum_pos = 0.0;
                    int count = 0;
                    for (const auto& s : node.succs) {
                        if (pos_map.find(s) != pos_map.end()) {
                            sum_pos += pos_map[s];
                            count++;
                        }
                    }
                    double bc = (count > 0) ? (sum_pos / count) : pos_map[nid];
                    barycenters.emplace_back(bc, pos_map[nid], nid);
                }

                std::sort(barycenters.begin(), barycenters.end(),
                          [](const auto& a, const auto& b) {
                              if (std::abs(std::get<0>(a) - std::get<0>(b)) > 1e-5) {
                                  return std::get<0>(a) < std::get<0>(b);
                              }
                              return std::get<1>(a) < std::get<1>(b);
                          });

                for (size_t i = 0; i < barycenters.size(); ++i) {
                    const std::string& nid = std::get<2>(barycenters[i]);
                    graph.ranks[r][i] = nid;
                    graph.nodes[nid].order = static_cast<int>(i);
                    pos_map[nid] = static_cast<double>(i);
                }
            }
        }
    }
}

} // namespace vajra::gui
