#include "vajra/liberty/models.hpp"

#include <cmath>

namespace vajra::liberty {

double LUT2D::lookup_bilinear(double var1_val, double var2_val) const {
    if (values.empty()) return 0.0;
    if (index_1.empty() && index_2.empty()) return values.front();

    // 1D interpolation if only index_1 is available
    if (index_2.empty() || index_2.size() == 1) {
        if (index_1.size() <= 1) return values.front();
        if (var1_val <= index_1.front()) return values.front();
        if (var1_val >= index_1.back()) return values.back();

        auto it = std::lower_bound(index_1.begin(), index_1.end(), var1_val);
        size_t i1 = std::distance(index_1.begin(), it);
        if (i1 == 0) return values.front();
        size_t i0 = i1 - 1;

        double x0 = index_1[i0], x1 = index_1[i1];
        double y0 = values[i0], y1 = values[i1];
        if (std::abs(x1 - x0) < 1e-12) return y0;
        double t = (var1_val - x0) / (x1 - x0);
        return y0 + t * (y1 - y0);
    }

    // 1D interpolation if only index_2 is available
    if (index_1.empty() || index_1.size() == 1) {
        if (index_2.size() <= 1) return values.front();
        if (var2_val <= index_2.front()) return values.front();
        if (var2_val >= index_2.back()) return values.back();

        auto it = std::lower_bound(index_2.begin(), index_2.end(), var2_val);
        size_t j1 = std::distance(index_2.begin(), it);
        if (j1 == 0) return values.front();
        size_t j0 = j1 - 1;

        double y0 = index_2[j0], y1 = index_2[j1];
        double z0 = values[j0], z1 = values[j1];
        if (std::abs(y1 - y0) < 1e-12) return z0;
        double u = (var2_val - y0) / (y1 - y0);
        return z0 + u * (z1 - z0);
    }

    // 2D Bilinear Interpolation
    size_t nr = index_1.size();
    size_t nc = index_2.size();

    // Clamp values to bounds for robust table lookup
    double v1 = std::clamp(var1_val, index_1.front(), index_1.back());
    double v2 = std::clamp(var2_val, index_2.front(), index_2.back());

    auto it1 = std::lower_bound(index_1.begin(), index_1.end(), v1);
    size_t r1 = std::distance(index_1.begin(), it1);
    if (r1 == 0) r1 = 1;
    if (r1 >= nr) r1 = nr - 1;
    size_t r0 = r1 - 1;

    auto it2 = std::lower_bound(index_2.begin(), index_2.end(), v2);
    size_t c1 = std::distance(index_2.begin(), it2);
    if (c1 == 0) c1 = 1;
    if (c1 >= nc) c1 = nc - 1;
    size_t c0 = c1 - 1;

    double x0 = index_1[r0], x1 = index_1[r1];
    double y0 = index_2[c0], y1 = index_2[c1];

    double dx = x1 - x0;
    double dy = y1 - y0;
    double t = (std::abs(dx) > 1e-12) ? ((v1 - x0) / dx) : 0.0;
    double u = (std::abs(dy) > 1e-12) ? ((v2 - y0) / dy) : 0.0;

    double q00 = at(r0, c0);
    double q01 = at(r0, c1);
    double q10 = at(r1, c0);
    double q11 = at(r1, c1);

    return (1.0 - t) * (1.0 - u) * q00 +
           t * (1.0 - u) * q10 +
           (1.0 - t) * u * q01 +
           t * u * q11;
}

std::string cell_class_to_string(CellClass c) {
    switch (c) {
        case CellClass::COMBINATIONAL: return "combinational";
        case CellClass::SEQUENTIAL: return "sequential";
        case CellClass::SCAN_CELL: return "scan_cell";
        case CellClass::CLOCK_GATING: return "clock_gating";
        case CellClass::MACRO_BLACKBOX: return "macro_blackbox";
        case CellClass::PAD: return "pad";
        case CellClass::TIE: return "tie";
        default: return "combinational";
    }
}

CellClass string_to_cell_class(std::string_view s) {
    if (s == "combinational") return CellClass::COMBINATIONAL;
    if (s == "sequential") return CellClass::SEQUENTIAL;
    if (s == "scan_cell") return CellClass::SCAN_CELL;
    if (s == "clock_gating") return CellClass::CLOCK_GATING;
    if (s == "macro_blackbox") return CellClass::MACRO_BLACKBOX;
    if (s == "pad") return CellClass::PAD;
    if (s == "tie") return CellClass::TIE;
    return CellClass::COMBINATIONAL;
}

std::string TimingArc::timing_type_to_string(TimingType t) {
    switch (t) {
        case TimingType::COMBINATIONAL: return "combinational";
        case TimingType::COMBINATIONAL_RISE: return "combinational_rise";
        case TimingType::COMBINATIONAL_FALL: return "combinational_fall";
        case TimingType::SETUP_RISING: return "setup_rising";
        case TimingType::SETUP_FALLING: return "setup_falling";
        case TimingType::HOLD_RISING: return "hold_rising";
        case TimingType::HOLD_FALLING: return "hold_falling";
        case TimingType::RECOVERY_RISING: return "recovery_rising";
        case TimingType::RECOVERY_FALLING: return "recovery_falling";
        case TimingType::REMOVAL_RISING: return "removal_rising";
        case TimingType::REMOVAL_FALLING: return "removal_falling";
        case TimingType::RISING_EDGE: return "rising_edge";
        case TimingType::FALLING_EDGE: return "falling_edge";
        case TimingType::MIN_PULSE_WIDTH: return "min_pulse_width";
        case TimingType::CLEAR: return "clear";
        case TimingType::PRESET: return "preset";
        default: return "unknown";
    }
}

TimingType TimingArc::string_to_timing_type(std::string_view s) {
    if (s == "combinational") return TimingType::COMBINATIONAL;
    if (s == "combinational_rise") return TimingType::COMBINATIONAL_RISE;
    if (s == "combinational_fall") return TimingType::COMBINATIONAL_FALL;
    if (s == "setup_rising") return TimingType::SETUP_RISING;
    if (s == "setup_falling") return TimingType::SETUP_FALLING;
    if (s == "hold_rising") return TimingType::HOLD_RISING;
    if (s == "hold_falling") return TimingType::HOLD_FALLING;
    if (s == "recovery_rising") return TimingType::RECOVERY_RISING;
    if (s == "recovery_falling") return TimingType::RECOVERY_FALLING;
    if (s == "removal_rising") return TimingType::REMOVAL_RISING;
    if (s == "removal_falling") return TimingType::REMOVAL_FALLING;
    if (s == "rising_edge") return TimingType::RISING_EDGE;
    if (s == "falling_edge") return TimingType::FALLING_EDGE;
    if (s == "min_pulse_width") return TimingType::MIN_PULSE_WIDTH;
    if (s == "clear") return TimingType::CLEAR;
    if (s == "preset") return TimingType::PRESET;
    return TimingType::UNKNOWN;
}

std::string TimingArc::timing_sense_to_string(TimingSense s) {
    switch (s) {
        case TimingSense::POSITIVE_UNATE: return "positive_unate";
        case TimingSense::NEGATIVE_UNATE: return "negative_unate";
        case TimingSense::NON_UNATE: return "non_unate";
        default: return "unknown";
    }
}

TimingSense TimingArc::string_to_timing_sense(std::string_view s) {
    if (s == "positive_unate") return TimingSense::POSITIVE_UNATE;
    if (s == "negative_unate") return TimingSense::NEGATIVE_UNATE;
    if (s == "non_unate") return TimingSense::NON_UNATE;
    return TimingSense::UNKNOWN;
}

std::string Pin::direction_to_string(PinDirection d) {
    switch (d) {
        case PinDirection::INPUT: return "input";
        case PinDirection::OUTPUT: return "output";
        case PinDirection::INOUT: return "inout";
        case PinDirection::INTERNAL: return "internal";
        default: return "unknown";
    }
}

PinDirection Pin::string_to_direction(std::string_view s) {
    if (s == "input") return PinDirection::INPUT;
    if (s == "output") return PinDirection::OUTPUT;
    if (s == "inout") return PinDirection::INOUT;
    if (s == "internal") return PinDirection::INTERNAL;
    return PinDirection::UNKNOWN;
}

const TimingArc* Pin::find_timing_arc(const std::string& rel_pin, TimingType type) const {
    for (const auto& arc : timing_arcs) {
        if (arc.related_pin == rel_pin && (type == TimingType::UNKNOWN || arc.timing_type == type)) {
            return &arc;
        }
    }
    return nullptr;
}

std::vector<const TimingArc*> Pin::get_timing_arcs(const std::string& rel_pin) const {
    std::vector<const TimingArc*> res;
    for (const auto& arc : timing_arcs) {
        if (rel_pin.empty() || arc.related_pin == rel_pin) {
            res.push_back(&arc);
        }
    }
    return res;
}

const Pin* Cell::find_pin(const std::string& pin_name) const {
    auto it = pins.find(pin_name);
    if (it != pins.end()) {
        return &it->second;
    }

    // Check if pin_name has no brackets but corresponds to a bus pin like "din0[127:0]" or "din0[0]"
    std::string prefix = pin_name + "[";
    for (const auto& [name, pin] : pins) {
        if (name.starts_with(prefix)) {
            return &pin;
        }
    }

    // Check if pin_name contains bracket indexing like "din0[5]" matching base "din0" or range "din0[127:0]"
    size_t brk = pin_name.find('[');
    if (brk != std::string::npos) {
        std::string base = pin_name.substr(0, brk);
        auto base_it = pins.find(base);
        if (base_it != pins.end()) {
            return &base_it->second;
        }
        std::string base_prefix = base + "[";
        for (const auto& [name, pin] : pins) {
            if (name.starts_with(base_prefix)) {
                return &pin;
            }
        }
    }

    return nullptr;
}

Pin* Cell::find_pin(const std::string& pin_name) {
    auto it = pins.find(pin_name);
    if (it != pins.end()) {
        return &it->second;
    }

    std::string prefix = pin_name + "[";
    for (auto& [name, pin] : pins) {
        if (name.starts_with(prefix)) {
            return &pin;
        }
    }

    size_t brk = pin_name.find('[');
    if (brk != std::string::npos) {
        std::string base = pin_name.substr(0, brk);
        auto base_it = pins.find(base);
        if (base_it != pins.end()) {
            return &base_it->second;
        }
        std::string base_prefix = base + "[";
        for (auto& [name, pin] : pins) {
            if (name.starts_with(base_prefix)) {
                return &pin;
            }
        }
    }

    return nullptr;
}

std::vector<std::string> Cell::get_input_pin_names() const {
    std::vector<std::string> result;
    for (const auto& [name, pin] : pins) {
        if (pin.direction == PinDirection::INPUT || pin.direction == PinDirection::INOUT) {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> Cell::get_output_pin_names() const {
    std::vector<std::string> result;
    for (const auto& [name, pin] : pins) {
        if (pin.direction == PinDirection::OUTPUT || pin.direction == PinDirection::INOUT) {
            result.push_back(name);
        }
    }
    return result;
}

const Pin* Cell::get_clock_pin() const {
    for (const auto& [name, pin] : pins) {
        if (pin.is_clock) return &pin;
    }
    // Infer from sequential blocks if not explicitly marked
    for (const auto& seq : sequential_blocks) {
        std::string clk_name = seq.clock_pin_name();
        const Pin* p = find_pin(clk_name);
        if (p) return p;
    }
    if (test_cell) {
        for (const auto& seq : test_cell->sequential_blocks) {
            std::string clk_name = seq.clock_pin_name();
            const Pin* p = find_pin(clk_name);
            if (p) return p;
        }
    }
    return nullptr;
}

const Cell* Library::find_cell(const std::string& cell_name) const {
    auto it = cells.find(cell_name);
    return (it != cells.end()) ? &it->second : nullptr;
}

Cell* Library::find_cell(const std::string& cell_name) {
    auto it = cells.find(cell_name);
    return (it != cells.end()) ? &it->second : nullptr;
}

const TableTemplate* Library::find_template(const std::string& tmpl_name) const {
    auto it = templates.find(tmpl_name);
    return (it != templates.end()) ? &it->second : nullptr;
}

CellClass Cell::infer_classification() const {
    if (is_scan_cell()) {
        return CellClass::SCAN_CELL;
    }

    std::string name_upper = name;
    for (char& c : name_upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    auto contains_kw = [&](const std::initializer_list<std::string_view>& kws) -> bool {
        for (const auto& kw : kws) {
            if (name_upper.find(kw) != std::string::npos) return true;
        }
        return false;
    };

    if (is_clock_gating || contains_kw({"ICG", "CLKGATE", "CLOCK_GATE", "CG_LAT", "CGLAT", "CKGATE"})) {
        return CellClass::CLOCK_GATING;
    }

    if (is_macro || contains_kw({"SRAM", "RAM", "ROM", "DPRAM", "SPRAM", "FIFO", "PLL", "DLL"})) {
        return CellClass::MACRO_BLACKBOX;
    }

    if (is_pad || contains_kw({"PAD_", "_PAD", "IOPAD"})) {
        return CellClass::PAD;
    }

    if (is_tie || contains_kw({"TIEHI", "TIELO", "TIE_HI", "TIE_LO", "TIE_HIGH", "TIE_LOW", "TIE_1", "TIE_0", "TIECELL"}) ||
        name_upper.rfind("TIE", 0) == 0) {
        return CellClass::TIE;
    }

    if (is_sequential()) {
        return CellClass::SEQUENTIAL;
    }

    return CellClass::COMBINATIONAL;
}

const OperatingCondition* Library::get_active_operating_condition() const {
    if (!default_operating_conditions.empty()) {
        auto it = op_conds.find(default_operating_conditions);
        if (it != op_conds.end()) return &it->second;
    }
    if (!op_conds.empty()) {
        return &op_conds.begin()->second;
    }
    return nullptr;
}

} // namespace vajra::liberty
