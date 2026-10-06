#pragma once

#include "vajra/liberty/models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vajra::liberty {

struct TimingResult {
    std::optional<double> cell_rise;        // Propagation delay rise (ps)
    std::optional<double> cell_fall;        // Propagation delay fall (ps)
    std::optional<double> rise_transition;  // Output slew rise (ps)
    std::optional<double> fall_transition;  // Output slew fall (ps)

    std::optional<double> delay_r() const { return cell_rise; }
    std::optional<double> delay_f() const { return cell_fall; }
    std::optional<double> slew_r() const { return rise_transition; }
    std::optional<double> slew_f() const { return fall_transition; }
};

struct ConstraintResult {
    std::optional<double> rise_constraint;  // Setup / hold constraint rise (ps)
    std::optional<double> fall_constraint;  // Setup / hold constraint fall (ps)

    std::optional<double> rise() const { return rise_constraint; }
    std::optional<double> fall() const { return fall_constraint; }
};

// Locates interval index for interpolation and tangent extrapolation (matches OpenSTA bisection)
size_t find_axis_index(double value, const std::vector<double>& values);

// Evaluates an NLDM 2D lookup table using bilinear interpolation with boundary tangent extrapolation
double interpolate_lut2d(const LUT2D& lut, double axis_val1, double axis_val2 = 0.0);

// Evaluates timing table dynamically mapping input_slew and load_cap to matching axes
double evaluate_timing_table(const LUT2D& lut, double input_slew, double load_cap);

// Evaluates constraint table dynamically mapping data_slew and clock_slew to matching axes
double evaluate_constraint_table(const LUT2D& lut, double data_slew, double clock_slew);

// Calculates propagation delay and output slew for a specific timing arc
TimingResult calculate_arc_timing(const TimingArc& arc, double input_slew, double load_cap);

// Calculates propagation delay and output slew across a cell from pin A to pin Y
std::optional<TimingResult> calculate_cell_timing(const Cell& cell,
                                                 const std::string& from_pin,
                                                 const std::string& to_pin,
                                                 double input_slew,
                                                 double load_cap,
                                                 const std::optional<std::string>& when = std::nullopt);

// Calculates setup / hold constraints for a sequential timing arc
ConstraintResult calculate_arc_constraint(const TimingArc& arc, double data_slew, double clock_slew);

} // namespace vajra::liberty
