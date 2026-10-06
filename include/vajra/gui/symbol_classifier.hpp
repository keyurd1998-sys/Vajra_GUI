#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace vajra::liberty {
struct Cell;
}

namespace vajra::gui {

enum class GateType {
    AND,
    NAND,
    OR,
    NOR,
    XOR,
    XNOR,
    NOT,
    BUF,
    MUX,
    DFF,
    SDFF,
    LATCH,
    PRIMARY_INPUT,
    PRIMARY_OUTPUT,
    MODULE_BOX
};

enum class PinRole {
    INPUT,
    OUTPUT,
    CLOCK,
    RESET,
    PRESET,
    ENABLE,
    SELECT,
    DATA,
    Q,
    QN
};

struct SymbolClassification {
    GateType gate_type{GateType::MODULE_BOX};
    bool is_inverting{false};
    bool has_clock{false};
    bool has_reset{false};
    bool has_preset{false};
    bool is_scan{false};
    bool is_latch{false};
    bool is_falling_edge{false};
    bool is_reset_active_low{true};
    bool is_preset_active_low{true};
    std::string clock_pin;
    std::string reset_pin;
    std::string preset_pin;
    std::string scan_in_pin;
    std::string scan_enable_pin;
    std::string data_pin;
    std::string output_pin;
    std::string qn_pin;
    std::vector<std::string> bubble_pins;
    std::unordered_map<std::string, PinRole> pin_roles;
    std::string description;
};

class SymbolClassifier {
public:
    static SymbolClassification classify(const std::string& cell_type,
                                         const std::unordered_map<std::string, std::string>& parameters = {});
    static SymbolClassification classify_from_liberty(const liberty::Cell& cell);
    static std::string gate_type_to_string(GateType type);
    static GateType string_to_gate_type(const std::string& str);
    static bool is_sequential(const std::string& cell_type);
    static bool is_output_pin(const std::string& cell_type, const std::string& pin_name);
    static bool is_input_pin(const std::string& cell_type, const std::string& pin_name);
};

} // namespace vajra::gui
