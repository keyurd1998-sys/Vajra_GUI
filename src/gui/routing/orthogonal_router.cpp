#include "vajra/gui/routing/orthogonal_router.hpp"
#include "vajra/gui/routing/pin_resolver.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>

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

std::pair<std::string, std::string> parse_pin_ref(const std::string& ref) {
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

double snap_coord(double val, double grid = 0.5) {
    return std::round(val / grid) * grid;
}

void add_segment(std::vector<RouteSegment>& segments, Point p1, Point p2, int track_id = 0, bool is_stub = false) {
    if (std::abs(p1.x - p2.x) < 1e-4 && std::abs(p1.y - p2.y) < 1e-4) return;

    RouteSegment seg;
    seg.p1 = p1;
    seg.p2 = p2;
    seg.track_id = track_id;
    seg.is_stub = is_stub;
    if (std::abs(p1.x - p2.x) < 1e-4) {
        seg.orientation = SegmentOrientation::VERTICAL;
    } else {
        seg.orientation = SegmentOrientation::HORIZONTAL;
    }
    segments.push_back(seg);
}

void normalize_segments(std::vector<RouteSegment>& segments) {
    if (segments.size() < 2) return;

    std::vector<RouteSegment> h_segs, v_segs, stubs;
    for (const auto& s : segments) {
        if (s.is_stub) {
            stubs.push_back(s);
        } else if (s.is_horizontal()) {
            h_segs.push_back(s);
        } else if (s.is_vertical()) {
            v_segs.push_back(s);
        }
    }

    // Merge horizontal collinear overlapping segments
    std::vector<RouteSegment> merged_h;
    std::map<double, std::vector<std::pair<double, double>>> h_by_y;
    for (const auto& s : h_segs) {
        double y = snap_coord(s.p1.y);
        double x1 = snap_coord(std::min(s.p1.x, s.p2.x));
        double x2 = snap_coord(std::max(s.p1.x, s.p2.x));
        if (x2 - x1 > 1e-3) {
            h_by_y[y].push_back({x1, x2});
        }
    }

    for (auto& [y, intervals] : h_by_y) {
        std::sort(intervals.begin(), intervals.end());
        double cur_start = intervals[0].first;
        double cur_end = intervals[0].second;
        for (size_t i = 1; i < intervals.size(); ++i) {
            if (intervals[i].first <= cur_end + 1e-2) {
                cur_end = std::max(cur_end, intervals[i].second);
            } else {
                RouteSegment seg;
                seg.p1 = {cur_start, y};
                seg.p2 = {cur_end, y};
                seg.orientation = SegmentOrientation::HORIZONTAL;
                merged_h.push_back(seg);
                cur_start = intervals[i].first;
                cur_end = intervals[i].second;
            }
        }
        RouteSegment seg;
        seg.p1 = {cur_start, y};
        seg.p2 = {cur_end, y};
        seg.orientation = SegmentOrientation::HORIZONTAL;
        merged_h.push_back(seg);
    }

    // Merge vertical collinear overlapping segments
    std::vector<RouteSegment> merged_v;
    std::map<double, std::vector<std::pair<double, double>>> v_by_x;
    for (const auto& s : v_segs) {
        double x = snap_coord(s.p1.x);
        double y1 = snap_coord(std::min(s.p1.y, s.p2.y));
        double y2 = snap_coord(std::max(s.p1.y, s.p2.y));
        if (y2 - y1 > 1e-3) {
            v_by_x[x].push_back({y1, y2});
        }
    }

    for (auto& [x, intervals] : v_by_x) {
        std::sort(intervals.begin(), intervals.end());
        double cur_start = intervals[0].first;
        double cur_end = intervals[0].second;
        for (size_t i = 1; i < intervals.size(); ++i) {
            if (intervals[i].first <= cur_end + 1e-2) {
                cur_end = std::max(cur_end, intervals[i].second);
            } else {
                RouteSegment seg;
                seg.p1 = {x, cur_start};
                seg.p2 = {x, cur_end};
                seg.orientation = SegmentOrientation::VERTICAL;
                merged_v.push_back(seg);
                cur_start = intervals[i].first;
                cur_end = intervals[i].second;
            }
        }
        RouteSegment seg;
        seg.p1 = {x, cur_start};
        seg.p2 = {x, cur_end};
        seg.orientation = SegmentOrientation::VERTICAL;
        merged_v.push_back(seg);
    }

    segments.clear();
    segments.insert(segments.end(), merged_h.begin(), merged_h.end());
    segments.insert(segments.end(), merged_v.begin(), merged_v.end());
    segments.insert(segments.end(), stubs.begin(), stubs.end());
}

std::vector<SolderDot> detect_solder_junctions(const std::vector<RouteSegment>& segments,
                                              const std::string& net_name,
                                              double radius = 3.5) {
    if (segments.size() < 2) return {};

    std::set<std::pair<double, double>> junctions;

    // 1. Degree counting on snapped coordinates
    std::map<std::pair<double, double>, int> deg_count;
    for (const auto& s : segments) {
        auto p1_key = std::make_pair(snap_coord(s.p1.x), snap_coord(s.p1.y));
        auto p2_key = std::make_pair(snap_coord(s.p2.x), snap_coord(s.p2.y));
        deg_count[p1_key]++;
        deg_count[p2_key]++;
    }

    for (const auto& [pt, count] : deg_count) {
        if (count >= 3) {
            junctions.insert(pt);
        }
    }

    // 2. T-junction detection
    std::vector<RouteSegment> v_segs;
    std::vector<RouteSegment> h_segs;
    for (const auto& s : segments) {
        if (s.is_vertical()) v_segs.push_back(s);
        else if (s.is_horizontal()) h_segs.push_back(s);
    }

    for (const auto& h : h_segs) {
        Point pts[2] = {h.p1, h.p2};
        for (const auto& p : pts) {
            double px = snap_coord(p.x);
            double py = snap_coord(p.y);
            for (const auto& v : v_segs) {
                double vx = snap_coord(v.p1.x);
                if (std::abs(px - vx) < 1e-2) {
                    double vy_min = snap_coord(std::min(v.p1.y, v.p2.y));
                    double vy_max = snap_coord(std::max(v.p1.y, v.p2.y));
                    if (vy_min + 1e-2 < py && py < vy_max - 1e-2) {
                        junctions.insert({px, py});
                    }
                }
            }
        }
    }

    for (const auto& v : v_segs) {
        Point pts[2] = {v.p1, v.p2};
        for (const auto& p : pts) {
            double px = snap_coord(p.x);
            double py = snap_coord(p.y);
            for (const auto& h : h_segs) {
                double hy = snap_coord(h.p1.y);
                if (std::abs(py - hy) < 1e-2) {
                    double hx_min = snap_coord(std::min(h.p1.x, h.p2.x));
                    double hx_max = snap_coord(std::max(h.p1.x, h.p2.x));
                    if (hx_min + 1e-2 < px && px < hx_max - 1e-2) {
                        junctions.insert({px, py});
                    }
                }
            }
        }
    }

    std::vector<SolderDot> dots;
    for (const auto& [x, y] : junctions) {
        dots.push_back({{x, y}, net_name, radius});
    }
    return dots;
}

} // anonymous namespace

RoutingResult OrthogonalRouter::route_all(const SchematicModule& module,
                                         const PlacementResult& placement,
                                         int hfn_threshold) {
    RoutingResult result;
    result.module_name = module.name;

    const auto& graph = placement.graph;
    int num_ranks = static_cast<int>(graph.ranks.size());
    PinResolver pin_resolver(graph, module);

    // Constants
    const double TRACK_GAP = 20.0;
    const double DOGLEG_OFFSET = 16.0;
    const double PERIMETER_MARGIN = 40.0;
    const double BBOX_PADDING = 25.0;
    const double track_pitch = 14.0;
    const double channel_margin = 16.0;

    // 1. Channel physical boundaries
    std::vector<std::pair<double, double>> channel_x_bounds;
    for (int r = 0; r < num_ranks - 1; ++r) {
        double x_right_curr = 50.0;
        for (const auto& nid : graph.ranks[r]) {
            const auto* n = graph.get_node(nid);
            if (n) x_right_curr = std::max(x_right_curr, n->x + n->width);
        }
        double x_left_next = x_right_curr + 120.0;
        if (r + 1 < num_ranks && !graph.ranks[r + 1].empty()) {
            x_left_next = 1e9;
            for (const auto& nid : graph.ranks[r + 1]) {
                const auto* n = graph.get_node(nid);
                if (n) x_left_next = std::min(x_left_next, n->x);
            }
        }
        channel_x_bounds.push_back({x_right_curr, x_left_next});
    }

    // Circuit BBox & perimeter corridors
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const auto& [_, n] : graph.nodes) {
        min_x = std::min(min_x, n.x);
        max_x = std::max(max_x, n.x + n.width);
        min_y = std::min(min_y, n.y);
        max_y = std::max(max_y, n.y + n.height);
    }
    if (graph.nodes.empty()) {
        min_x = 50.0; max_x = 500.0; min_y = 50.0; max_y = 500.0;
    }

    double left_perimeter_x = min_x - PERIMETER_MARGIN;
    double right_perimeter_x = max_x + PERIMETER_MARGIN;
    double top_corridor_base = std::min(25.0, min_y - PERIMETER_MARGIN);
    double bottom_corridor_base = max_y + PERIMETER_MARGIN;
    double mid_y = (min_y + max_y) / 2.0;

    int top_corridor_next_track = 0;
    int bottom_corridor_next_track = 0;
    int left_perimeter_next_track = 0;
    int right_perimeter_next_track = 0;

    // Spatial node index sorted by Y for fast horizontal collision checks
    std::vector<const PlacementNode*> y_sorted_nodes;
    for (const auto& [_, n] : graph.nodes) {
        y_sorted_nodes.push_back(&n);
    }
    std::sort(y_sorted_nodes.begin(), y_sorted_nodes.end(), [](const auto* a, const auto* b) {
        return a->y < b->y;
    });

    auto check_horizontal_collision = [&](double x1, double x2, double y, double margin = 4.0) -> bool {
        double xa = std::min(x1, x2);
        double xb = std::max(x1, x2);
        for (const auto* node : y_sorted_nodes) {
            if (node->y > y + margin) break;
            if (node->y + node->height + margin < y) continue;
            if (std::max(xa, node->x) < std::min(xb, node->x + node->width) - 1.0) {
                return true;
            }
        }
        return false;
    };

    // Helper to test if net is HFN
    auto is_hfn_net = [&](const std::string& nname, size_t fanout, bool is_clk, bool is_rst) -> bool {
        if (static_cast<int>(fanout) >= hfn_threshold) return true;
        std::string ln = to_lower(nname);
        if (is_clk || ln.find("clk") != std::string::npos || ln.find("clock") != std::string::npos) return true;
        if (is_rst || ln.find("rst") != std::string::npos || ln.find("reset") != std::string::npos) return true;
        if (ln.find("scan_en") != std::string::npos || ln.find("test_mode") != std::string::npos) return true;
        return false;
    };

    // 2. Identify HFNs and collect regular nets
    struct NetSpec {
        std::string name;
        PinLocation src;
        std::vector<PinLocation> dsts;
        bool is_clock{false};
        bool is_reset{false};
    };

    std::vector<NetSpec> hfn_nets;
    std::vector<NetSpec> regular_nets;

    std::vector<std::string> sorted_net_names;
    for (const auto& [nname, _] : module.nets) {
        sorted_net_names.push_back(nname);
    }
    std::sort(sorted_net_names.begin(), sorted_net_names.end());

    std::vector<NetRoute> constant_tie_routes;

    for (const auto& net_name : sorted_net_names) {
        const auto& net = module.nets.at(net_name);
        if (net.sink_pins.empty()) continue;

        if (net.driver_pin.empty()) {
            std::vector<PinLocation> p_dsts;
            for (const auto& sref : net.sink_pins) {
                auto [snk_owner, snk_pin] = parse_pin_ref(sref);
                const auto* pd = pin_resolver.get_pin(snk_owner, snk_pin);
                if (pd) p_dsts.push_back(*pd);
            }
            if (p_dsts.empty()) continue;

            NetRoute nr;
            nr.net_name = net_name;
            nr.is_high_fanout = false;
            nr.is_clock = false;
            nr.is_reset = false;

            std::string label = net_name;
            if (label == "1'0" || label == "0") label = "1'b0";
            else if (label == "1'1" || label == "1") label = "1'b1";
            else if (label == "1'x") label = "1'bx";
            else if (label == "1'z") label = "1'bz";

            double stub_len = 16.0;
            for (const auto& pd : p_dsts) {
                Point s_start{pd.x - stub_len, pd.y};
                Point s_end{pd.x, pd.y};
                std::string s_arrow = "RIGHT";

                if (pd.direction == PinExitDirection::SOUTH) {
                    s_start = {pd.x, pd.y + stub_len};
                    s_end = {pd.x, pd.y};
                    s_arrow = "UP";
                } else if (pd.direction == PinExitDirection::EAST) {
                    s_start = {pd.x + stub_len, pd.y};
                    s_end = {pd.x, pd.y};
                    s_arrow = "LEFT";
                } else if (pd.direction == PinExitDirection::NORTH) {
                    s_start = {pd.x, pd.y - stub_len};
                    s_end = {pd.x, pd.y};
                    s_arrow = "DOWN";
                }

                add_segment(nr.segments, s_start, s_end, 0, true);
                nr.hfn_stubs.push_back({net_name, pd, s_start, s_end, label, false, s_arrow});
                nr.sink_pins.push_back(pd);
            }

            if (!nr.segments.empty()) {
                constant_tie_routes.push_back(std::move(nr));
            }
            continue;
        }

        auto [drv_owner, drv_pin] = parse_pin_ref(net.driver_pin);
        const auto* p_src = pin_resolver.get_pin(drv_owner, drv_pin);
        if (!p_src) continue;

        std::vector<PinLocation> p_dsts;
        for (const auto& sref : net.sink_pins) {
            auto [snk_owner, snk_pin] = parse_pin_ref(sref);
            const auto* pd = pin_resolver.get_pin(snk_owner, snk_pin);
            if (pd) p_dsts.push_back(*pd);
        }
        if (p_dsts.empty()) continue;

        NetSpec spec{net_name, *p_src, p_dsts, net.is_clock, net.is_reset};

        if (is_hfn_net(net_name, p_dsts.size(), net.is_clock, net.is_reset)) {
            hfn_nets.push_back(spec);
        } else {
            regular_nets.push_back(spec);
        }
    }

    // 3. Pre-plan routes and allocate unique corridor tracks
    struct NetRoutingPlan {
        NetSpec spec;
        int r_src{0};
        int c_src{0};
        bool needs_corridor{false};
        bool use_top_corridor{true};
        double corridor_y{0.0};
        std::map<int, std::vector<PinLocation>> dest_rank_sinks;
        std::vector<PinLocation> adjacent_sinks;
    };

    std::vector<NetRoutingPlan> plans;
    for (const auto& spec : regular_nets) {
        NetRoutingPlan plan;
        plan.spec = spec;
        const auto* src_node = graph.get_node(spec.src.node_id);
        plan.r_src = src_node ? src_node->rank : 0;
        plan.c_src = std::max(0, std::min(plan.r_src, num_ranks - 2));

        for (const auto& pd : spec.dsts) {
            const auto* dst_node = graph.get_node(pd.node_id);
            int r_dst = dst_node ? dst_node->rank : 0;
            if (r_dst == plan.r_src + 1 && pd.x > spec.src.x) {
                plan.adjacent_sinks.push_back(pd);
            } else {
                plan.dest_rank_sinks[r_dst].push_back(pd);
                plan.needs_corridor = true;
            }
        }

        if (plan.needs_corridor) {
            plan.use_top_corridor = (spec.src.y < mid_y);
            if (plan.use_top_corridor) {
                plan.corridor_y = top_corridor_base - (top_corridor_next_track++ * 16.0);
            } else {
                plan.corridor_y = bottom_corridor_base + (bottom_corridor_next_track++ * 16.0);
            }
        }
        plans.push_back(std::move(plan));
    }

    // 4. Collect vertical intervals across ALL channels for Left-Edge
    std::unordered_map<int, std::vector<std::tuple<double, double, std::string>>> channel_intervals;

    for (const auto& plan : plans) {
        if (num_ranks <= 1) continue;

        // A. Source trunk in channel c_src
        if (plan.r_src < num_ranks - 1) {
            double y_min = plan.spec.src.y;
            double y_max = plan.spec.src.y;
            for (const auto& pd : plan.adjacent_sinks) {
                double target_y = (pd.direction == PinExitDirection::SOUTH) ? (pd.y + DOGLEG_OFFSET) : pd.y;
                y_min = std::min(y_min, target_y);
                y_max = std::max(y_max, target_y);
            }
            if (plan.needs_corridor) {
                y_min = std::min(y_min, plan.corridor_y);
                y_max = std::max(y_max, plan.corridor_y);
            }
            channel_intervals[plan.c_src].emplace_back(y_min, y_max, plan.spec.name + "::src");
        }

        // B. Destination drop trunks in channel c_drop for each destination rank
        for (const auto& [r_dst, sinks] : plan.dest_rank_sinks) {
            if (r_dst > 0) {
                int c_drop = std::max(0, std::min(r_dst - 1, num_ranks - 2));
                double y_min = plan.corridor_y;
                double y_max = plan.corridor_y;
                for (const auto& pd : sinks) {
                    double target_y = (pd.direction == PinExitDirection::SOUTH) ? (pd.y + DOGLEG_OFFSET) : pd.y;
                    y_min = std::min(y_min, target_y);
                    y_max = std::max(y_max, target_y);
                }
                std::string key = plan.spec.name + "::dest::" + std::to_string(r_dst);
                channel_intervals[c_drop].emplace_back(y_min, y_max, key);
            }
        }
    }

    // 5. Left-Edge Channel Track Assignment
    std::unordered_map<int, std::unordered_map<std::string, int>> channel_track_alloc;
    std::unordered_map<int, int> channel_track_count;

    for (auto& [c_idx, intervals] : channel_intervals) {
        std::sort(intervals.begin(), intervals.end(), [](const auto& a, const auto& b) {
            if (std::abs(std::get<0>(a) - std::get<0>(b)) > 1e-3) {
                return std::get<0>(a) < std::get<0>(b);
            }
            return std::get<1>(a) < std::get<1>(b);
        });

        using HeapElem = std::pair<double, int>;
        std::priority_queue<HeapElem, std::vector<HeapElem>, std::greater<HeapElem>> heap;
        int track_count = 0;

        for (const auto& [ymin, ymax, key] : intervals) {
            int track_id = 0;
            if (!heap.empty() && heap.top().first + TRACK_GAP <= ymin) {
                track_id = heap.top().second;
                heap.pop();
            } else {
                track_id = track_count++;
            }
            heap.push({ymax, track_id});
            channel_track_alloc[c_idx][key] = track_id;
        }
        channel_track_count[c_idx] = std::max(1, track_count);
    }

    auto get_channel_track_x = [&](int channel_idx, int track_id) -> double {
        if (channel_x_bounds.empty()) return 100.0;
        int c = std::max(0, std::min(channel_idx, static_cast<int>(channel_x_bounds.size()) - 1));
        double x_start = channel_x_bounds[c].first;
        double x_end = channel_x_bounds[c].second;
        double trunk_start = x_start + channel_margin;
        double trunk_end = x_end - channel_margin;
        double avail_width = std::max(20.0, trunk_end - trunk_start);

        int total_tracks = std::max(1, channel_track_count[c]);
        if (total_tracks <= 1) {
            return (trunk_start + trunk_end) / 2.0;
        }
        double pitch = std::min(track_pitch, avail_width / std::max(1, total_tracks - 1));
        return trunk_start + track_id * pitch;
    };

    // 6. Route High-Fanout Nets (HFNs) with decoupled local stubs
    for (const auto& spec : hfn_nets) {
        NetRoute nr;
        nr.net_name = spec.name;
        nr.is_high_fanout = true;
        nr.is_clock = spec.is_clock;
        nr.is_reset = spec.is_reset;
        nr.driver_pin = spec.src;
        nr.sink_pins = spec.dsts;

        // Driver stub
        const double stub_len = 16.0;
        Point p_start{spec.src.x, spec.src.y};
        Point p_end{spec.src.x + stub_len, spec.src.y};
        std::string arrow = "RIGHT";

        if (spec.src.direction == PinExitDirection::SOUTH) {
            p_end = {spec.src.x, spec.src.y + stub_len};
            arrow = "DOWN";
        } else if (spec.src.direction == PinExitDirection::WEST) {
            p_end = {spec.src.x - stub_len, spec.src.y};
            arrow = "LEFT";
        } else if (spec.src.direction == PinExitDirection::NORTH) {
            p_end = {spec.src.x, spec.src.y - stub_len};
            arrow = "UP";
        }

        add_segment(nr.segments, p_start, p_end, 0, true);
        nr.hfn_stubs.push_back({spec.name, spec.src, p_start, p_end, spec.name, true, arrow});

        // Sink stubs
        for (const auto& pd : spec.dsts) {
            Point s_start{pd.x - stub_len, pd.y};
            Point s_end{pd.x, pd.y};
            std::string s_arrow = "RIGHT";

            if (pd.direction == PinExitDirection::SOUTH) {
                s_start = {pd.x, pd.y + stub_len};
                s_end = {pd.x, pd.y};
                s_arrow = "UP";
            } else if (pd.direction == PinExitDirection::EAST) {
                s_start = {pd.x + stub_len, pd.y};
                s_end = {pd.x, pd.y};
                s_arrow = "LEFT";
            } else if (pd.direction == PinExitDirection::NORTH) {
                s_start = {pd.x, pd.y - stub_len};
                s_end = {pd.x, pd.y};
                s_arrow = "DOWN";
            }

            add_segment(nr.segments, s_start, s_end, 0, true);
            nr.hfn_stubs.push_back({spec.name, pd, s_start, s_end, spec.name, false, s_arrow});
        }

        result.routes.push_back(std::move(nr));
        result.decoupled_hfn_count++;
    }

    result.routes.insert(result.routes.end(),
                         std::make_move_iterator(constant_tie_routes.begin()),
                         std::make_move_iterator(constant_tie_routes.end()));

    // Register HFN horizontal stubs
    struct HorizontalLead {
        double y;
        double x_min;
        double x_max;
        std::string net_name;
    };
    std::vector<HorizontalLead> global_h_leads;

    for (const auto& r : result.routes) {
        for (const auto& s : r.segments) {
            if (s.is_horizontal()) {
                global_h_leads.push_back({s.p1.y, std::min(s.p1.x, s.p2.x), std::max(s.p1.x, s.p2.x), r.net_name});
            }
        }
    }

    auto add_horizontal_lead = [&](std::vector<RouteSegment>& segments,
                                   std::vector<double>& trunk_y_points,
                                   double x_trunk,
                                   double x_pin,
                                   double y_pin,
                                   const std::string& net_name,
                                   bool is_driver) {
        double x_start = std::min(x_trunk, x_pin);
        double x_end = std::max(x_trunk, x_pin);
        if (x_end - x_start < 1e-3) return;

        // Check if [x_start, x_end] at y_pin collides with an existing horizontal lead of another net
        bool collides = false;
        for (const auto& lead : global_h_leads) {
            if (lead.net_name == net_name) continue;
            if (std::abs(lead.y - y_pin) < 1.0) {
                double ov_min = std::max(x_start, lead.x_min);
                double ov_max = std::min(x_end, lead.x_max);
                if (ov_max - ov_min > 2.0) {
                    collides = true;
                    break;
                }
            }
        }

        if (!collides) {
            trunk_y_points.push_back(y_pin);
            add_segment(segments, {x_trunk, y_pin}, {x_pin, y_pin});
            global_h_leads.push_back({y_pin, x_start, x_end, net_name});
            return;
        }

        // Collision detected! Find a free jog Y offset: try +8, -8, +16, -16, +24, -24...
        double chosen_y = y_pin;
        for (double offset : {8.0, -8.0, 16.0, -16.0, 24.0, -24.0, 32.0, -32.0}) {
            double cand_y = y_pin + offset;
            bool cand_collides = false;
            for (const auto& lead : global_h_leads) {
                if (lead.net_name == net_name) continue;
                if (std::abs(lead.y - cand_y) < 1.0) {
                    double ov_min = std::max(x_start, lead.x_min);
                    double ov_max = std::min(x_end, lead.x_max);
                    if (ov_max - ov_min > 2.0) {
                        cand_collides = true;
                        break;
                    }
                }
            }
            if (!cand_collides) {
                chosen_y = cand_y;
                break;
            }
        }

        trunk_y_points.push_back(chosen_y);
        double dist = std::abs(x_pin - x_trunk);
        double jog_dx = std::min(8.0, dist / 2.0);

        if (is_driver) {
            double x_jog = (x_trunk > x_pin) ? (x_pin + jog_dx) : (x_pin - jog_dx);
            add_segment(segments, {x_pin, y_pin}, {x_jog, y_pin});
            global_h_leads.push_back({y_pin, std::min(x_pin, x_jog), std::max(x_pin, x_jog), net_name});
            add_segment(segments, {x_jog, y_pin}, {x_jog, chosen_y});
            add_segment(segments, {x_jog, chosen_y}, {x_trunk, chosen_y});
            global_h_leads.push_back({chosen_y, std::min(x_jog, x_trunk), std::max(x_jog, x_trunk), net_name});
        } else {
            double x_jog = (x_pin > x_trunk) ? (x_pin - jog_dx) : (x_pin + jog_dx);
            add_segment(segments, {x_trunk, chosen_y}, {x_jog, chosen_y});
            global_h_leads.push_back({chosen_y, std::min(x_trunk, x_jog), std::max(x_trunk, x_jog), net_name});
            add_segment(segments, {x_jog, chosen_y}, {x_jog, y_pin});
            add_segment(segments, {x_jog, y_pin}, {x_pin, y_pin});
            global_h_leads.push_back({y_pin, std::min(x_jog, x_pin), std::max(x_jog, x_pin), net_name});
        }
    };

    // 7. Route Regular Nets
    for (const auto& plan : plans) {
        NetRoute nr;
        nr.net_name = plan.spec.name;
        nr.is_high_fanout = false;
        nr.is_clock = plan.spec.is_clock;
        nr.is_reset = plan.spec.is_reset;
        nr.driver_pin = plan.spec.src;
        nr.sink_pins = plan.spec.dsts;

        // Optimization: Single sink straight horizontal line with no obstacles
        if (plan.spec.dsts.size() == 1 &&
            plan.adjacent_sinks.size() == 1 &&
            std::abs(plan.spec.src.y - plan.spec.dsts[0].y) < 1e-3 &&
            plan.spec.dsts[0].x > plan.spec.src.x &&
            plan.spec.dsts[0].direction == PinExitDirection::WEST) {
            if (!check_horizontal_collision(plan.spec.src.x, plan.spec.dsts[0].x, plan.spec.src.y)) {
                bool straight_collides = false;
                double xa = plan.spec.src.x;
                double xb = plan.spec.dsts[0].x;
                for (const auto& lead : global_h_leads) {
                    if (std::abs(lead.y - plan.spec.src.y) < 1.0) {
                        if (std::min(xb, lead.x_max) - std::max(xa, lead.x_min) > 2.0) {
                            straight_collides = true;
                            break;
                        }
                    }
                }
                if (!straight_collides) {
                    add_segment(nr.segments, {plan.spec.src.x, plan.spec.src.y}, {plan.spec.dsts[0].x, plan.spec.dsts[0].y});
                    global_h_leads.push_back({plan.spec.src.y, xa, xb, plan.spec.name});
                    result.routes.push_back(std::move(nr));
                    result.regular_routed_count++;
                    continue;
                }
            }
        }

        // Main trunk X coordinate
        double x_src_trunk = 0.0;
        int track_src = 0;
        if (plan.r_src >= num_ranks - 1) {
            x_src_trunk = right_perimeter_x + (right_perimeter_next_track++ * 16.0);
        } else {
            std::string key = plan.spec.name + "::src";
            track_src = channel_track_alloc[plan.c_src][key];
            x_src_trunk = get_channel_track_x(plan.c_src, track_src);
        }

        std::vector<double> src_trunk_y_points;

        // Driver exit lead
        if (std::abs(plan.spec.src.x - x_src_trunk) > 1e-3) {
            add_horizontal_lead(nr.segments, src_trunk_y_points, x_src_trunk, plan.spec.src.x, plan.spec.src.y, plan.spec.name, true);
        } else {
            src_trunk_y_points.push_back(plan.spec.src.y);
        }

        // Connect adjacent sinks directly from source trunk
        for (const auto& pd : plan.adjacent_sinks) {
            if (pd.direction == PinExitDirection::SOUTH) {
                double y_dogleg = pd.y + DOGLEG_OFFSET;
                add_horizontal_lead(nr.segments, src_trunk_y_points, x_src_trunk, pd.x, y_dogleg, plan.spec.name, false);
                add_segment(nr.segments, {pd.x, y_dogleg}, {pd.x, pd.y});
            } else {
                add_horizontal_lead(nr.segments, src_trunk_y_points, x_src_trunk, pd.x, pd.y, plan.spec.name, false);
            }
        }

        // Connect corridor sinks (feedback and skip-rank)
        if (plan.needs_corridor) {
            double y_corr = plan.corridor_y;
            src_trunk_y_points.push_back(y_corr);

            for (const auto& [r_dst, sinks] : plan.dest_rank_sinks) {
                double x_dest_trunk = 0.0;
                int track_dest = 0;

                if (r_dst == 0) {
                    x_dest_trunk = left_perimeter_x - (left_perimeter_next_track++ * 16.0);
                } else {
                    int c_drop = std::max(0, std::min(r_dst - 1, num_ranks - 2));
                    std::string key = plan.spec.name + "::dest::" + std::to_string(r_dst);
                    track_dest = channel_track_alloc[c_drop][key];
                    x_dest_trunk = get_channel_track_x(c_drop, track_dest);
                }

                // Horizontal segment along corridor from source trunk to drop trunk
                add_segment(nr.segments, {x_src_trunk, y_corr}, {x_dest_trunk, y_corr});

                // Drop trunk in destination channel: spans from y_corr to all sinks in this rank
                std::vector<double> dest_trunk_y_points = {y_corr};
                for (const auto& pd : sinks) {
                    if (pd.direction == PinExitDirection::SOUTH) {
                        double y_dogleg = pd.y + DOGLEG_OFFSET;
                        add_horizontal_lead(nr.segments, dest_trunk_y_points, x_dest_trunk, pd.x, y_dogleg, plan.spec.name, false);
                        add_segment(nr.segments, {pd.x, y_dogleg}, {pd.x, pd.y});
                    } else {
                        add_horizontal_lead(nr.segments, dest_trunk_y_points, x_dest_trunk, pd.x, pd.y, plan.spec.name, false);
                    }
                }

                double y_min_dest = dest_trunk_y_points.front();
                double y_max_dest = dest_trunk_y_points.front();
                for (double y : dest_trunk_y_points) {
                    y_min_dest = std::min(y_min_dest, y);
                    y_max_dest = std::max(y_max_dest, y);
                }
                if (y_max_dest - y_min_dest > 1e-3) {
                    add_segment(nr.segments, {x_dest_trunk, y_min_dest}, {x_dest_trunk, y_max_dest}, track_dest);
                }
            }
        }

        // Draw main source trunk
        double y_min_src = src_trunk_y_points.front();
        double y_max_src = src_trunk_y_points.front();
        for (double y : src_trunk_y_points) {
            y_min_src = std::min(y_min_src, y);
            y_max_src = std::max(y_max_src, y);
        }
        if (y_max_src - y_min_src > 1e-3) {
            add_segment(nr.segments, {x_src_trunk, y_min_src}, {x_src_trunk, y_max_src}, track_src);
        }

        // Normalize segments (deduplicate and merge collinear segments)
        normalize_segments(nr.segments);

        // Detect solder junctions on normalized segments
        if (plan.spec.dsts.size() > 1) {
            nr.solder_dots = detect_solder_junctions(nr.segments, plan.spec.name);
        }

        result.routes.push_back(std::move(nr));
        result.regular_routed_count++;
    }

    // Compute updated bounding box including all corridor and perimeter tracks
    double bbox_min_x = min_x - PERIMETER_MARGIN - (left_perimeter_next_track * 16.0) - BBOX_PADDING;
    double bbox_max_x = max_x + PERIMETER_MARGIN + (right_perimeter_next_track * 16.0) + BBOX_PADDING;
    double bbox_min_y = top_corridor_base - (top_corridor_next_track * 16.0) - BBOX_PADDING;
    double bbox_max_y = bottom_corridor_base + (bottom_corridor_next_track * 16.0) + BBOX_PADDING;

    for (const auto& r : result.routes) {
        for (const auto& s : r.segments) {
            bbox_min_x = std::min({bbox_min_x, s.p1.x, s.p2.x});
            bbox_max_x = std::max({bbox_max_x, s.p1.x, s.p2.x});
            bbox_min_y = std::min({bbox_min_y, s.p1.y, s.p2.y});
            bbox_max_y = std::max({bbox_max_y, s.p1.y, s.p2.y});
        }
    }

    result.bbox = {bbox_min_x - BBOX_PADDING, bbox_min_y - BBOX_PADDING,
                   (bbox_max_x - bbox_min_x) + 2.0 * BBOX_PADDING,
                   (bbox_max_y - bbox_min_y) + 2.0 * BBOX_PADDING};

    // Compute metrics
    result.total_segments = 0;
    result.total_solder_dots = 0;
    result.total_bends = 0;
    result.is_strictly_orthogonal = true;

    for (const auto& r : result.routes) {
        result.total_segments += static_cast<int>(r.segments.size());
        result.total_solder_dots += static_cast<int>(r.solder_dots.size());
        for (const auto& s : r.segments) {
            if (!s.is_orthogonal()) {
                result.is_strictly_orthogonal = false;
            }
        }
    }

    result.bbox = placement.bbox;
    return result;
}

std::vector<NetRoute> OrthogonalRouter::route(const SchematicModule& module,
                                             const PlacementResult& placement,
                                             int hfn_threshold) {
    auto res = route_all(module, placement, hfn_threshold);
    return std::move(res.routes);
}

} // namespace vajra::gui
