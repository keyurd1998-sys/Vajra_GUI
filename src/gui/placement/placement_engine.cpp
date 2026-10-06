#include "vajra/gui/placement/placement_engine.hpp"

namespace vajra::gui {

PlacementResult PlacementEngine::run(const SchematicModule& module,
                                    int num_crossing_iterations,
                                    double col_spacing,
                                    double row_spacing,
                                    bool center_align,
                                    bool decouple_dff) {
    PlacementResult result;
    result.module_name = module.name;

    // Stage 1: Cycle Breaking & DAG Construction
    result.graph = CycleBreaker::build_dag(module, decouple_dff);

    // Count statistics
    result.total_nodes = static_cast<int>(result.graph.nodes.size());
    result.total_edges = static_cast<int>(result.graph.edges.size());
    for (const auto& e : result.graph.edges) {
        if (e.is_feedback) {
            result.feedback_edges++;
        }
    }

    if (result.total_nodes == 0) {
        return result;
    }

    // Stage 2: Topological Layering
    result.num_ranks = LayerAssigner::assign_layers(result.graph) + 1;

    // Stage 3: Barycentric Crossing Minimization
    CrossingMinimizer::minimize(result.graph, num_crossing_iterations);

    // Stage 4: Coordinate and Channel Assignment
    result.bbox = CoordinateAssigner::assign_coordinates(result.graph, col_spacing, row_spacing, center_align);

    return result;
}

} // namespace vajra::gui
