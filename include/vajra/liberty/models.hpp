#pragma once

#include "vajra/liberty/units.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vajra::liberty {

// 2D Look-Up Table Template (lu_table_template)
struct TableTemplate {
    std::string name;
    std::string variable_1; // e.g. "total_output_net_capacitance"
    std::string variable_2; // e.g. "input_net_transition"
    std::vector<double> index_1;
    std::vector<double> index_2;
};

// 2D Look-Up Table Instance (NLDM Matrix)
struct LUT2D {
    std::string template_name;
    std::string variable_1;
    std::string variable_2;
    std::vector<double> index_1;
    std::vector<double> index_2;
    std::vector<double> values; // Row-major flattened array of size rows() * cols()

    size_t rows() const { return index_1.size(); }
    size_t cols() const { return index_2.size(); }
    bool empty() const { return values.empty(); }

    double at(size_t r, size_t c) const {
        size_t c_size = index_2.empty() ? 1 : index_2.size();
        size_t idx = r * c_size + c;
        if (idx < values.size()) {
            return values[idx];
        }
        return 0.0;
    }

    void set(size_t r, size_t c, double val) {
        size_t c_size = index_2.empty() ? 1 : index_2.size();
        size_t idx = r * c_size + c;
        if (idx >= values.size()) {
            values.resize(idx + 1, 0.0);
        }
        values[idx] = val;
    }

    // Bilinear interpolation across (var1_val, var2_val)
    double lookup_bilinear(double var1_val, double var2_val) const;
};

// Functional cell classification matching OpenDFT standard
enum class CellClass {
    COMBINATIONAL,
    SEQUENTIAL,
    SCAN_CELL,
    CLOCK_GATING,
    MACRO_BLACKBOX,
    PAD,
    TIE
};

std::string cell_class_to_string(CellClass c);
CellClass string_to_cell_class(std::string_view s);

enum class TimingType {
    COMBINATIONAL,
    COMBINATIONAL_RISE,
    COMBINATIONAL_FALL,
    SETUP_RISING,
    SETUP_FALLING,
    HOLD_RISING,
    HOLD_FALLING,
    RECOVERY_RISING,
    RECOVERY_FALLING,
    REMOVAL_RISING,
    REMOVAL_FALLING,
    RISING_EDGE,
    FALLING_EDGE,
    MIN_PULSE_WIDTH,
    CLEAR,
    PRESET,
    UNKNOWN
};

enum class TimingSense {
    POSITIVE_UNATE,
    NEGATIVE_UNATE,
    NON_UNATE,
    UNKNOWN
};

struct TimingArc {
    TimingType timing_type{TimingType::COMBINATIONAL};
    TimingSense timing_sense{TimingSense::UNKNOWN};
    std::string related_pin;
    std::string when; // Boolean condition string

    // 2D NLDM Lookup Tables
    std::optional<LUT2D> cell_rise;
    std::optional<LUT2D> cell_fall;
    std::optional<LUT2D> rise_transition;
    std::optional<LUT2D> fall_transition;
    std::optional<LUT2D> rise_constraint;
    std::optional<LUT2D> fall_constraint;

    static std::string timing_type_to_string(TimingType t);
    static TimingType string_to_timing_type(std::string_view s);
    static std::string timing_sense_to_string(TimingSense s);
    static TimingSense string_to_timing_sense(std::string_view s);
};

enum class PinDirection {
    INPUT,
    OUTPUT,
    INOUT,
    INTERNAL,
    UNKNOWN
};

struct Pin {
    std::string name;
    PinDirection direction{PinDirection::UNKNOWN};
    double capacitance{0.0};       // Normalized to fF
    double rise_capacitance{0.0};  // Normalized to fF
    double fall_capacitance{0.0};  // Normalized to fF
    double max_capacitance{0.0};   // Normalized to fF
    double min_capacitance{0.0};   // Normalized to fF
    double max_transition{0.0};    // Normalized to ps
    std::string function_str;      // Boolean pin function logic string
    bool is_clock{false};
    std::vector<TimingArc> timing_arcs;

    static std::string direction_to_string(PinDirection d);
    static PinDirection string_to_direction(std::string_view s);

    const TimingArc* find_timing_arc(const std::string& rel_pin, TimingType type = TimingType::COMBINATIONAL) const;
    std::vector<const TimingArc*> get_timing_arcs(const std::string& rel_pin = "") const;
};

// Sequential block: ff (IQ, IQN) or latch (IQ, IQN)
struct SequentialBlock {
    std::string var1; // e.g. "IQ"
    std::string var2; // e.g. "IQN"
    std::string clocked_on; // e.g. "CLK", "CLK'"
    std::string next_state; // e.g. "D", "D & EN"
    std::string clear;      // e.g. "RESET", "!RESET"
    std::string preset;     // e.g. "SET"
    std::string clear_preset_var1;
    std::string clear_preset_var2;
    bool is_latch{false};

    bool clock_is_inverted() const {
        if (clocked_on.empty()) return false;
        return clocked_on.front() == '!' || clocked_on.front() == '~' || clocked_on.back() == '\'';
    }

    std::string clock_pin_name() const {
        std::string s = clocked_on;
        while (!s.empty() && (s.front() == '!' || s.front() == '~' || s.front() == ' ')) {
            s.erase(s.begin());
        }
        while (!s.empty() && (s.back() == '\'' || s.back() == ' ')) {
            s.pop_back();
        }
        return s;
    }
};

// Scan test cell specification
struct TestCell {
    std::string scan_in;     // e.g. "SI", "TI"
    std::string scan_enable; // e.g. "SE", "TE"
    std::string scan_out;    // e.g. "Q", "SO"
    std::string data_in;     // e.g. "D"
    std::string clock;       // e.g. "CLK"
    std::vector<SequentialBlock> sequential_blocks;
};

// Scan replacement pairing info matching OpenDFT ScanReplacementInfo
struct ScanReplacementInfo {
    std::string non_scan_name;
    std::string scan_name;
    std::string scan_in{"SI"};
    std::string scan_enable{"SE"};
    std::string scan_out{"Q"};
    std::string data_in{"D"};
    std::string clock{"CLK"};
};

struct OperatingCondition {
    std::string name;
    double process{1.0};
    double voltage{1.0};     // Volts
    double temperature{25.0}; // Celsius
};

struct Cell {
    std::string name;
    double area{0.0};
    std::string cell_footprint;
    double leakage_power{0.0};
    bool dont_touch{false};
    bool dont_use{false};
    bool is_macro{false};
    bool is_pad{false};
    bool is_clock_gating{false};
    bool is_tie{false};
    CellClass cell_class{CellClass::COMBINATIONAL};

    std::map<std::string, Pin> pins;
    std::vector<SequentialBlock> sequential_blocks;
    std::optional<TestCell> test_cell;
    std::map<std::string, std::string> attributes;

    const Pin* find_pin(const std::string& pin_name) const;
    Pin* find_pin(const std::string& pin_name);

    bool is_sequential() const {
        return !sequential_blocks.empty();
    }

    bool is_scan_cell() const {
        return test_cell.has_value() || cell_class == CellClass::SCAN_CELL;
    }

    bool is_combinational() const {
        return !is_sequential();
    }

    CellClass infer_classification() const;
    CellClass classification() const { return cell_class; }

    std::vector<std::string> get_input_pin_names() const;
    std::vector<std::string> get_output_pin_names() const;
    const Pin* get_clock_pin() const;
};

struct Library {
    std::string name;
    std::string filepath;
    Units units;
    std::map<std::string, TableTemplate> templates;
    std::map<std::string, Cell> cells;
    std::map<std::string, OperatingCondition> op_conds;
    std::string default_operating_conditions;
    std::map<std::string, std::string> attributes;

    const Cell* find_cell(const std::string& cell_name) const;
    Cell* find_cell(const std::string& cell_name);
    const TableTemplate* find_template(const std::string& tmpl_name) const;
    size_t cell_count() const { return cells.size(); }
    const OperatingCondition* get_active_operating_condition() const;
};

} // namespace vajra::liberty
