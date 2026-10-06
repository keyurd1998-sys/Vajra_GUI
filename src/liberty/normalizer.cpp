#include "vajra/liberty/normalizer.hpp"

#include <algorithm>
#include <cctype>

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

bool is_time_variable(std::string_view var) {
    std::string l = to_lower(var);
    return (l.find("transition") != std::string::npos ||
            l.find("slew") != std::string::npos ||
            l.find("time") != std::string::npos);
}

bool is_capacitance_variable(std::string_view var) {
    std::string l = to_lower(var);
    return (l.find("capacitance") != std::string::npos ||
            l.find("load") != std::string::npos ||
            l.find("cap") != std::string::npos);
}

void scale_table_axes(LUT2D& lut, double time_scale, double cap_scale) {
    if (is_time_variable(lut.variable_1)) {
        for (double& v : lut.index_1) v *= time_scale;
    } else if (is_capacitance_variable(lut.variable_1)) {
        for (double& v : lut.index_1) v *= cap_scale;
    }

    if (is_time_variable(lut.variable_2)) {
        for (double& v : lut.index_2) v *= time_scale;
    } else if (is_capacitance_variable(lut.variable_2)) {
        for (double& v : lut.index_2) v *= cap_scale;
    }
}

void scale_lut(LUT2D& lut, double time_scale, double cap_scale) {
    scale_table_axes(lut, time_scale, cap_scale);
    for (double& v : lut.values) {
        v *= time_scale;
    }
}

} // namespace

void normalize_units(Library& lib) {
    double time_scale = lib.units.time_scale_ps;
    double cap_scale = lib.units.cap_scale_ff;

    // Scale template grid breakpoints
    for (auto& [name, tmpl] : lib.templates) {
        if (is_time_variable(tmpl.variable_1)) {
            for (double& v : tmpl.index_1) v *= time_scale;
        } else if (is_capacitance_variable(tmpl.variable_1)) {
            for (double& v : tmpl.index_1) v *= cap_scale;
        }

        if (is_time_variable(tmpl.variable_2)) {
            for (double& v : tmpl.index_2) v *= time_scale;
        } else if (is_capacitance_variable(tmpl.variable_2)) {
            for (double& v : tmpl.index_2) v *= cap_scale;
        }
    }

    // Scale cell pins and timing tables
    for (auto& [cell_name, cell] : lib.cells) {
        for (auto& [pin_name, pin] : cell.pins) {
            pin.capacitance *= cap_scale;
            pin.rise_capacitance *= cap_scale;
            pin.fall_capacitance *= cap_scale;
            pin.max_capacitance *= cap_scale;
            pin.min_capacitance *= cap_scale;
            pin.max_transition *= time_scale;

            for (auto& arc : pin.timing_arcs) {
                if (arc.cell_rise) scale_lut(*arc.cell_rise, time_scale, cap_scale);
                if (arc.cell_fall) scale_lut(*arc.cell_fall, time_scale, cap_scale);
                if (arc.rise_transition) scale_lut(*arc.rise_transition, time_scale, cap_scale);
                if (arc.fall_transition) scale_lut(*arc.fall_transition, time_scale, cap_scale);
                if (arc.rise_constraint) scale_lut(*arc.rise_constraint, time_scale, cap_scale);
                if (arc.fall_constraint) scale_lut(*arc.fall_constraint, time_scale, cap_scale);
            }
        }
    }

    // Reset scales to 1.0 to ensure normalization is idempotent
    lib.units.time_scale_ps = 1.0;
    lib.units.cap_scale_ff = 1.0;
}

} // namespace vajra::liberty
