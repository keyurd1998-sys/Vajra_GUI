#include "vajra/liberty/timing.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace vajra::liberty {

namespace {

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool is_transition_var(std::string_view var) {
    std::string l = to_lower(var);
    return (l.find("transition") != std::string::npos ||
            l.find("slew") != std::string::npos ||
            l.find("time") != std::string::npos);
}

bool is_clock_var(std::string_view var) {
    std::string l = to_lower(var);
    return (l.find("related_pin_transition") != std::string::npos ||
            l.find("clock") != std::string::npos);
}

} // namespace

size_t find_axis_index(double value, const std::vector<double>& values) {
    size_t n = values.size();
    if (n <= 1 || value <= values[0]) return 0;
    if (value >= values.back()) return (n >= 2) ? (n - 2) : 0;
    auto it = std::upper_bound(values.begin(), values.end(), value);
    size_t idx = std::distance(values.begin(), it);
    return (idx > 0) ? (idx - 1) : 0;
}

double interpolate_lut2d(const LUT2D& lut, double axis_val1, double axis_val2) {
    size_t m = lut.rows();
    size_t n = lut.cols();

    if (m == 0 || n == 0 || lut.values.empty()) return 0.0;

    // 1. Scalar table (1x1)
    if (m == 1 && n == 1) {
        return lut.at(0, 0);
    }

    // 2. 1D table along Axis 1
    if (m == 1) {
        size_t j = find_axis_index(axis_val2, lut.index_2);
        double c_lo = lut.index_2[j];
        double c_hi = lut.index_2[j + 1];
        double dc = (std::abs(c_hi - c_lo) > 1e-12) ? ((axis_val2 - c_lo) / (c_hi - c_lo)) : 0.0;
        return (1.0 - dc) * lut.at(0, j) + dc * lut.at(0, j + 1);
    }

    // 3. 1D table along Axis 0
    if (n == 1) {
        size_t i = find_axis_index(axis_val1, lut.index_1);
        double s_lo = lut.index_1[i];
        double s_hi = lut.index_1[i + 1];
        double ds = (std::abs(s_hi - s_lo) > 1e-12) ? ((axis_val1 - s_lo) / (s_hi - s_lo)) : 0.0;
        return (1.0 - ds) * lut.at(i, 0) + ds * lut.at(i + 1, 0);
    }

    // 4. Standard 2D table (M x N) with bilinear interpolation and tangent extrapolation
    size_t i = find_axis_index(axis_val1, lut.index_1);
    size_t j = find_axis_index(axis_val2, lut.index_2);

    double s_lo = lut.index_1[i];
    double s_hi = lut.index_1[i + 1];
    double c_lo = lut.index_2[j];
    double c_hi = lut.index_2[j + 1];

    double ds = (std::abs(s_hi - s_lo) > 1e-12) ? ((axis_val1 - s_lo) / (s_hi - s_lo)) : 0.0;
    double dc = (std::abs(c_hi - c_lo) > 1e-12) ? ((axis_val2 - c_lo) / (c_hi - c_lo)) : 0.0;

    double y00 = lut.at(i, j);
    double y10 = lut.at(i + 1, j);
    double y01 = lut.at(i, j + 1);
    double y11 = lut.at(i + 1, j + 1);

    return (1.0 - ds) * (1.0 - dc) * y00
         + ds * (1.0 - dc) * y10
         + (1.0 - ds) * dc * y01
         + ds * dc * y11;
}

double evaluate_timing_table(const LUT2D& lut, double input_slew, double load_cap) {
    if (is_transition_var(lut.variable_1)) {
        return interpolate_lut2d(lut, input_slew, load_cap);
    }
    return interpolate_lut2d(lut, load_cap, input_slew);
}

double evaluate_constraint_table(const LUT2D& lut, double data_slew, double clock_slew) {
    if (is_clock_var(lut.variable_1)) {
        return interpolate_lut2d(lut, clock_slew, data_slew);
    }
    return interpolate_lut2d(lut, data_slew, clock_slew);
}

TimingResult calculate_arc_timing(const TimingArc& arc, double input_slew, double load_cap) {
    TimingResult res;
    if (arc.cell_rise) {
        res.cell_rise = evaluate_timing_table(*arc.cell_rise, input_slew, load_cap);
    }
    if (arc.cell_fall) {
        res.cell_fall = evaluate_timing_table(*arc.cell_fall, input_slew, load_cap);
    }
    if (arc.rise_transition) {
        res.rise_transition = evaluate_timing_table(*arc.rise_transition, input_slew, load_cap);
    }
    if (arc.fall_transition) {
        res.fall_transition = evaluate_timing_table(*arc.fall_transition, input_slew, load_cap);
    }
    return res;
}

std::optional<TimingResult> calculate_cell_timing(const Cell& cell,
                                                 const std::string& from_pin,
                                                 const std::string& to_pin,
                                                 double input_slew,
                                                 double load_cap,
                                                 const std::optional<std::string>& when) {
    const Pin* pin = cell.find_pin(to_pin);
    if (!pin) return std::nullopt;

    std::vector<const TimingArc*> arcs = pin->get_timing_arcs(from_pin);
    if (arcs.empty()) return std::nullopt;

    if (when.has_value() && !when->empty()) {
        std::vector<const TimingArc*> filtered;
        for (const auto* a : arcs) {
            if (a->when == *when) filtered.push_back(a);
        }
        if (!filtered.empty()) {
            arcs = std::move(filtered);
        }
    }

    std::optional<TimingResult> best_res;
    for (const auto* arc : arcs) {
        TimingResult res = calculate_arc_timing(*arc, input_slew, load_cap);
        if (!best_res) {
            best_res = res;
        } else {
            auto cr = (res.cell_rise && best_res->cell_rise) ? std::max(*res.cell_rise, *best_res->cell_rise)
                      : (res.cell_rise ? res.cell_rise : best_res->cell_rise);
            auto cf = (res.cell_fall && best_res->cell_fall) ? std::max(*res.cell_fall, *best_res->cell_fall)
                      : (res.cell_fall ? res.cell_fall : best_res->cell_fall);
            auto rt = (res.rise_transition && best_res->rise_transition) ? std::max(*res.rise_transition, *best_res->rise_transition)
                      : (res.rise_transition ? res.rise_transition : best_res->rise_transition);
            auto ft = (res.fall_transition && best_res->fall_transition) ? std::max(*res.fall_transition, *best_res->fall_transition)
                      : (res.fall_transition ? res.fall_transition : best_res->fall_transition);
            best_res = TimingResult{cr, cf, rt, ft};
        }
    }
    return best_res;
}

ConstraintResult calculate_arc_constraint(const TimingArc& arc, double data_slew, double clock_slew) {
    ConstraintResult res;
    if (arc.rise_constraint) {
        res.rise_constraint = evaluate_constraint_table(*arc.rise_constraint, data_slew, clock_slew);
    }
    if (arc.fall_constraint) {
        res.fall_constraint = evaluate_constraint_table(*arc.fall_constraint, data_slew, clock_slew);
    }
    return res;
}

} // namespace vajra::liberty
