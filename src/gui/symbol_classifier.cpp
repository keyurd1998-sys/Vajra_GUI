#include "vajra/gui/symbol_classifier.hpp"
#include "vajra/liberty/models.hpp"
#include "vajra/liberty/bool_expr.hpp"

#include <algorithm>
#include <cctype>

namespace vajra::gui {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // anonymous namespace

SymbolClassification SymbolClassifier::classify(
    const std::string& cell_type,
    const std::unordered_map<std::string, std::string>& /*parameters*/) {

    SymbolClassification result;
    std::string lower = to_lower(cell_type);

    // RTLIL Primitives only. Any technology/library cell MUST be resolved from Liberty (.lib).
    if (lower == "$_not_" || lower == "$_inv_" || lower == "$not") {
        result.gate_type = GateType::NOT;
        result.is_inverting = true;
        result.data_pin = "A";
        result.output_pin = "Y";
        result.bubble_pins.push_back("Y");
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "Inverter (NOT Gate)";
        return result;
    }

    if (lower == "$_buf_" || lower == "$buf") {
        result.gate_type = GateType::BUF;
        result.data_pin = "A";
        result.output_pin = "Y";
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "Buffer";
        return result;
    }

    if (lower == "$_nand_" || lower == "$nand") {
        result.gate_type = GateType::NAND;
        result.is_inverting = true;
        result.output_pin = "Y";
        result.bubble_pins.push_back("Y");
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "NAND Gate";
        return result;
    }

    if (lower == "$_and_" || lower == "$and") {
        result.gate_type = GateType::AND;
        result.output_pin = "Y";
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "AND Gate";
        return result;
    }

    if (lower == "$_xnor_" || lower == "$xnor") {
        result.gate_type = GateType::XNOR;
        result.is_inverting = true;
        result.output_pin = "Y";
        result.bubble_pins.push_back("Y");
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "XNOR Gate";
        return result;
    }

    if (lower == "$_xor_" || lower == "$xor") {
        result.gate_type = GateType::XOR;
        result.output_pin = "Y";
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "XOR Gate";
        return result;
    }

    if (lower == "$_nor_" || lower == "$nor") {
        result.gate_type = GateType::NOR;
        result.is_inverting = true;
        result.output_pin = "Y";
        result.bubble_pins.push_back("Y");
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "NOR Gate";
        return result;
    }

    if (lower == "$_or_" || lower == "$or") {
        result.gate_type = GateType::OR;
        result.output_pin = "Y";
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "OR Gate";
        return result;
    }

    if (lower == "$_mux_" || lower == "$mux") {
        result.gate_type = GateType::MUX;
        result.output_pin = "Y";
        result.pin_roles["A"] = PinRole::INPUT;
        result.pin_roles["B"] = PinRole::INPUT;
        result.pin_roles["S"] = PinRole::SELECT;
        result.pin_roles["Y"] = PinRole::OUTPUT;
        result.description = "Multiplexer (MUX 2:1)";
        return result;
    }

    // Check for DFF / Flip-Flops in RTLIL primitives
    if (lower.rfind("$_dff", 0) == 0 || lower.rfind("$_sdff", 0) == 0 ||
        lower.rfind("$_aldff", 0) == 0 || lower.rfind("$_dffe", 0) == 0 ||
        lower == "$dff" || lower == "$adff" || lower == "$sdff" || lower == "$dffe") {
        bool is_scan = (lower.rfind("$_sdff", 0) == 0 || lower == "$sdff");
        result.gate_type = is_scan ? GateType::SDFF : GateType::DFF;
        result.is_scan = is_scan;
        result.has_clock = true;
        result.clock_pin = "C";
        result.data_pin = "D";
        result.output_pin = "Q";
        result.pin_roles["C"] = PinRole::CLOCK;
        result.pin_roles["CLK"] = PinRole::CLOCK;
        result.pin_roles["D"] = PinRole::DATA;
        result.pin_roles["Q"] = PinRole::Q;
        if (cell_type.find("_N") != std::string::npos || lower.find("dff_n") != std::string::npos) {
            result.is_falling_edge = true;
            result.bubble_pins.push_back("C");
        }
        if (cell_type.find("_P") != std::string::npos || cell_type.find("_N") != std::string::npos) {
            result.has_reset = true;
            result.reset_pin = "R";
            result.pin_roles["R"] = PinRole::RESET;
            if (lower.find("_pn") != std::string::npos || lower.find("_nn") != std::string::npos ||
                lower.find("_p0") != std::string::npos || lower.find("_n0") != std::string::npos) {
                result.is_reset_active_low = true;
                result.bubble_pins.push_back("R");
            }
        }
        result.description = is_scan ? "Scan D Flip-Flop" : "D Flip-Flop";
        return result;
    }

    // Check for Latch in RTLIL primitives
    if (lower.rfind("$_dlatch", 0) == 0 || lower == "$dlatch") {
        result.gate_type = GateType::LATCH;
        result.is_latch = true;
        result.has_clock = true;
        result.clock_pin = "E";
        result.data_pin = "D";
        result.output_pin = "Q";
        result.pin_roles["E"] = PinRole::ENABLE;
        result.pin_roles["D"] = PinRole::DATA;
        result.pin_roles["Q"] = PinRole::Q;
        result.description = "D Latch";
        return result;
    }

    // Default: Generic Module / Blackbox
    result.gate_type = GateType::MODULE_BOX;
    result.description = "Generic Module / Blackbox";
    return result;
}

std::string SymbolClassifier::gate_type_to_string(GateType type) {
    switch (type) {
        case GateType::AND: return "AND";
        case GateType::NAND: return "NAND";
        case GateType::OR: return "OR";
        case GateType::NOR: return "NOR";
        case GateType::XOR: return "XOR";
        case GateType::XNOR: return "XNOR";
        case GateType::NOT: return "NOT";
        case GateType::BUF: return "BUF";
        case GateType::MUX: return "MUX";
        case GateType::DFF: return "DFF";
        case GateType::SDFF: return "SDFF";
        case GateType::LATCH: return "LATCH";
        case GateType::PRIMARY_INPUT: return "INPUT";
        case GateType::PRIMARY_OUTPUT: return "OUTPUT";
        case GateType::MODULE_BOX: return "MODULE";
    }
    return "MODULE";
}

GateType SymbolClassifier::string_to_gate_type(const std::string& str) {
    if (str == "AND") return GateType::AND;
    if (str == "NAND") return GateType::NAND;
    if (str == "OR") return GateType::OR;
    if (str == "NOR") return GateType::NOR;
    if (str == "XOR") return GateType::XOR;
    if (str == "XNOR") return GateType::XNOR;
    if (str == "NOT") return GateType::NOT;
    if (str == "BUF") return GateType::BUF;
    if (str == "MUX") return GateType::MUX;
    if (str == "DFF") return GateType::DFF;
    if (str == "SDFF") return GateType::SDFF;
    if (str == "LATCH") return GateType::LATCH;
    if (str == "INPUT") return GateType::PRIMARY_INPUT;
    if (str == "OUTPUT") return GateType::PRIMARY_OUTPUT;
    return GateType::MODULE_BOX;
}

SymbolClassification SymbolClassifier::classify_from_liberty(const liberty::Cell& cell) {
    SymbolClassification result;
    result.description = cell.name + " (Liberty: " + cell_class_to_string(cell.cell_class) + ")";

    // 1. Assign pin roles & directions from Liberty pins
    for (const auto& [pname, pin] : cell.pins) {
        if (pin.direction == liberty::PinDirection::INPUT) {
            result.pin_roles[pname] = PinRole::INPUT;
        } else if (pin.direction == liberty::PinDirection::OUTPUT) {
            result.pin_roles[pname] = PinRole::OUTPUT;
            if (result.output_pin.empty()) {
                result.output_pin = pname;
            }
        }
        if (pin.is_clock) {
            result.pin_roles[pname] = PinRole::CLOCK;
            result.has_clock = true;
            result.clock_pin = pname;
        }
    }

    // 2. Sequential Blocks (Flip-Flops and Latches)
    if (cell.is_sequential() || !cell.sequential_blocks.empty()) {
        const auto& seq = cell.sequential_blocks.front();
        if (seq.is_latch) {
            result.gate_type = GateType::LATCH;
            result.is_latch = true;
        } else {
            result.gate_type = GateType::DFF;
        }

        result.has_clock = true;
        std::string clk_name = seq.clock_pin_name();
        if (!clk_name.empty()) {
            result.clock_pin = clk_name;
            result.pin_roles[clk_name] = PinRole::CLOCK;
        } else {
            for (const auto& [pname, pin] : cell.pins) {
                if (pin.is_clock) {
                    result.clock_pin = pname;
                    result.pin_roles[pname] = PinRole::CLOCK;
                    break;
                }
            }
        }
        if (seq.clock_is_inverted()) {
            result.is_falling_edge = true;
            if (!result.clock_pin.empty()) {
                result.bubble_pins.push_back(result.clock_pin);
            }
        }

        if (!seq.clear.empty()) {
            result.has_reset = true;
            std::string rst_name = seq.clear;
            while (!rst_name.empty() && (rst_name.front() == '!' || rst_name.front() == '~' || rst_name.front() == ' ')) {
                rst_name.erase(rst_name.begin());
            }
            while (!rst_name.empty() && (rst_name.back() == '\'' || rst_name.back() == ' ')) {
                rst_name.pop_back();
            }
            result.reset_pin = rst_name;
            result.pin_roles[rst_name] = PinRole::RESET;
            bool active_low = (seq.clear.front() == '!' || seq.clear.front() == '~' || seq.clear.back() == '\'' ||
                               rst_name.ends_with("_B") || rst_name.ends_with("_b") ||
                               rst_name.ends_with("_N") || rst_name.ends_with("_n") ||
                               rst_name.find("rst_n") != std::string::npos || rst_name.find("reset_n") != std::string::npos);
            result.is_reset_active_low = active_low;
            if (active_low) {
                result.bubble_pins.push_back(rst_name);
            }
        }

        if (!seq.preset.empty()) {
            result.has_preset = true;
            std::string pre_name = seq.preset;
            while (!pre_name.empty() && (pre_name.front() == '!' || pre_name.front() == '~' || pre_name.front() == ' ')) {
                pre_name.erase(pre_name.begin());
            }
            while (!pre_name.empty() && (pre_name.back() == '\'' || pre_name.back() == ' ')) {
                pre_name.pop_back();
            }
            result.preset_pin = pre_name;
            result.pin_roles[pre_name] = PinRole::PRESET;
            bool active_low = (seq.preset.front() == '!' || seq.preset.front() == '~' || seq.preset.back() == '\'' ||
                               pre_name.ends_with("_B") || pre_name.ends_with("_b") ||
                               pre_name.ends_with("_N") || pre_name.ends_with("_n") ||
                               pre_name.find("set_n") != std::string::npos || pre_name.find("pre_n") != std::string::npos);
            result.is_preset_active_low = active_low;
            if (active_low) {
                result.bubble_pins.push_back(pre_name);
            }
        }

        // Identify Scan Flip-Flop features
        bool is_scan_cell = (cell.cell_class == liberty::CellClass::SCAN_CELL);
        std::string lower_cname = to_lower(cell.name);
        if (lower_cname.find("sdf") != std::string::npos || lower_cname.find("scan") != std::string::npos) {
            is_scan_cell = true;
        }

        std::string si_pin, se_pin;
        for (const auto& [pname, pin] : cell.pins) {
            std::string lp = to_lower(pname);
            if (lp == "si" || lp == "scd" || lp == "ti" || lp == "sdi" || lp == "scan_in") {
                si_pin = pname;
                is_scan_cell = true;
            } else if (lp == "se" || lp == "sce" || lp == "te" || lp == "sen" || lp == "scan_en" || lp == "scan_enable") {
                se_pin = pname;
                is_scan_cell = true;
            }
        }

        if (cell.test_cell.has_value()) {
            const auto& tc = *cell.test_cell;
            if (!tc.scan_in.empty() && cell.pins.count(tc.scan_in)) {
                si_pin = tc.scan_in;
                is_scan_cell = true;
            }
            if (!tc.scan_enable.empty() && cell.pins.count(tc.scan_enable)) {
                se_pin = tc.scan_enable;
                is_scan_cell = true;
            }
        }

        if (!seq.next_state.empty()) {
            std::string lns = to_lower(seq.next_state);
            if ((lns.find("&") != std::string::npos && lns.find("|") != std::string::npos) ||
                lns.find("sce") != std::string::npos || lns.find("scd") != std::string::npos) {
                is_scan_cell = true;
            }
        }

        if (is_scan_cell && !result.is_latch) {
            result.gate_type = GateType::SDFF;
            result.is_scan = true;
            result.scan_in_pin = si_pin;
            result.scan_enable_pin = se_pin;
            if (!si_pin.empty()) result.pin_roles[si_pin] = PinRole::DATA;
            if (!se_pin.empty()) result.pin_roles[se_pin] = PinRole::ENABLE;
        }

        if (!seq.var1.empty()) {
            for (const auto& [pname, pin] : cell.pins) {
                if (pin.function_str == seq.var1 || pname == "Q" || pin.direction == liberty::PinDirection::OUTPUT) {
                    result.pin_roles[pname] = PinRole::Q;
                    result.output_pin = pname;
                    break;
                }
            }
        }

        if (!seq.var2.empty()) {
            for (const auto& [pname, pin] : cell.pins) {
                if (pin.function_str == seq.var2 || pname == "QN" || pname == "Q_N" || pname == "QB") {
                    result.qn_pin = pname;
                    result.pin_roles[pname] = PinRole::QN;
                    result.bubble_pins.push_back(pname);
                    break;
                }
            }
        }

        if (!seq.next_state.empty()) {
            for (const auto& [pname, pin] : cell.pins) {
                if (pname == seq.next_state || pname == "D") {
                    result.data_pin = pname;
                    result.pin_roles[pname] = PinRole::DATA;
                    break;
                }
            }
        }

        if (result.is_latch) {
            result.description = "Latch (" + cell.name + ")";
        } else if (result.is_scan) {
            result.description = "Scan D Flip-Flop (" + cell.name + ")";
        } else {
            result.description = "D Flip-Flop (" + cell.name + ")";
        }

        return result;
    }

    // 3. Macros & Blackbox Blocks
    if (cell.is_macro || cell.area > 500.0) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    // 4. Combinational Cells: Query output pin function
    std::vector<const liberty::Pin*> output_pins;
    for (const auto& [_, pin] : cell.pins) {
        if (pin.direction == liberty::PinDirection::OUTPUT) {
            output_pins.push_back(&pin);
        }
    }

    if (output_pins.size() != 1) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    const auto* out_pin = output_pins.front();
    if (out_pin->function_str.empty()) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    result.output_pin = out_pin->name;

    // Parse Liberty Boolean function string
    auto expr = liberty::BoolExprParser::parse(out_pin->function_str);
    if (!expr) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    std::vector<std::string> vars = expr->get_variables();
    size_t n = vars.size();

    if (n == 0) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    if (n == 1) {
        const std::string& v = vars[0];
        std::unordered_map<std::string, bool> env0{{v, false}};
        std::unordered_map<std::string, bool> env1{{v, true}};
        bool f0 = expr->evaluate(env0);
        bool f1 = expr->evaluate(env1);

        if (f0 && !f1) {
            result.gate_type = GateType::NOT;
            result.is_inverting = true;
            result.bubble_pins.push_back(out_pin->name);
            result.data_pin = v;
            result.pin_roles[v] = PinRole::DATA;
            result.description = "Inverter (NOT Gate)";
            return result;
        } else if (!f0 && f1) {
            result.gate_type = GateType::BUF;
            result.is_inverting = false;
            result.data_pin = v;
            result.pin_roles[v] = PinRole::DATA;
            result.description = "Buffer";
            return result;
        }
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    if (n > 8) {
        result.gate_type = GateType::MODULE_BOX;
        return result;
    }

    // Evaluate canonical truth table across all 2^N rows
    auto tt = expr->truth_table(vars);
    size_t total_rows = tt.size();
    size_t true_count = 0;
    for (bool b : tt) {
        if (b) ++true_count;
    }
    size_t false_count = total_rows - true_count;

    // Check N-input AND: true ONLY when all inputs are 1 (last row)
    if (true_count == 1 && tt.back() == true) {
        result.gate_type = GateType::AND;
        result.is_inverting = false;
        result.description = std::to_string(n) + "-input AND Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    // Check N-input NAND: false ONLY when all inputs are 1 (last row)
    if (false_count == 1 && tt.back() == false) {
        result.gate_type = GateType::NAND;
        result.is_inverting = true;
        result.bubble_pins.push_back(out_pin->name);
        result.description = std::to_string(n) + "-input NAND Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    // Check N-input OR: false ONLY when all inputs are 0 (first row)
    if (false_count == 1 && tt.front() == false) {
        result.gate_type = GateType::OR;
        result.is_inverting = false;
        result.description = std::to_string(n) + "-input OR Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    // Check N-input NOR: true ONLY when all inputs are 0 (first row)
    if (true_count == 1 && tt.front() == true) {
        result.gate_type = GateType::NOR;
        result.is_inverting = true;
        result.bubble_pins.push_back(out_pin->name);
        result.description = std::to_string(n) + "-input NOR Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    // Check XOR / XNOR (parity across all N inputs)
    bool is_xor = true;
    bool is_xnor = true;
    for (size_t r = 0; r < total_rows; ++r) {
        size_t ones = 0;
        for (size_t i = 0; i < n; ++i) {
            if ((r >> (n - 1 - i)) & 1) ++ones;
        }
        bool odd_parity = (ones % 2 != 0);
        if (tt[r] != odd_parity) is_xor = false;
        if (tt[r] != !odd_parity) is_xnor = false;
    }

    if (is_xor) {
        result.gate_type = GateType::XOR;
        result.is_inverting = false;
        result.description = std::to_string(n) + "-input XOR Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    if (is_xnor) {
        result.gate_type = GateType::XNOR;
        result.is_inverting = true;
        result.bubble_pins.push_back(out_pin->name);
        result.description = std::to_string(n) + "-input XNOR Gate";
        for (const auto& v : vars) result.pin_roles[v] = PinRole::INPUT;
        return result;
    }

    // Check 2:1 Multiplexer (n == 3: 1 select line, 2 data lines)
    if (n == 3) {
        for (size_t s_idx = 0; s_idx < 3; ++s_idx) {
            const std::string& s_var = vars[s_idx];
            std::vector<std::string> other_vars;
            for (size_t i = 0; i < 3; ++i) {
                if (i != s_idx) other_vars.push_back(vars[i]);
            }
            const std::string& v0 = other_vars[0];
            const std::string& v1 = other_vars[1];

            for (const auto& [d0, d1] : {std::make_pair(v0, v1), std::make_pair(v1, v0)}) {
                bool is_mux = true;
                bool is_inv_mux = true;
                for (bool val_s : {false, true}) {
                    const std::string& sel_d = val_s ? d1 : d0;
                    for (bool val_v0 : {false, true}) {
                        for (bool val_v1 : {false, true}) {
                            std::unordered_map<std::string, bool> env;
                            env[s_var] = val_s;
                            env[v0] = val_v0;
                            env[v1] = val_v1;
                            bool out_val = expr->evaluate(env);
                            if (out_val != env[sel_d]) is_mux = false;
                            if (out_val != !env[sel_d]) is_inv_mux = false;
                        }
                    }
                }
                if (is_mux) {
                    result.gate_type = GateType::MUX;
                    result.is_inverting = false;
                    result.pin_roles[s_var] = PinRole::SELECT;
                    result.pin_roles[d0] = PinRole::DATA;
                    result.pin_roles[d1] = PinRole::DATA;
                    result.description = "2:1 Multiplexer";
                    return result;
                }
                if (is_inv_mux) {
                    result.gate_type = GateType::MUX;
                    result.is_inverting = true;
                    result.bubble_pins.push_back(out_pin->name);
                    result.pin_roles[s_var] = PinRole::SELECT;
                    result.pin_roles[d0] = PinRole::DATA;
                    result.pin_roles[d1] = PinRole::DATA;
                    result.description = "Inverting 2:1 Multiplexer";
                    return result;
                }
            }
        }
    }

    // Check 4:1 Multiplexer (n == 6: 2 select lines, 4 data lines)
    if (n == 6) {
        for (size_t s0_idx = 0; s0_idx < 6; ++s0_idx) {
            for (size_t s1_idx = s0_idx + 1; s1_idx < 6; ++s1_idx) {
                const std::string& s0 = vars[s0_idx];
                const std::string& s1 = vars[s1_idx];
                std::vector<std::string> data_vars;
                for (size_t i = 0; i < 6; ++i) {
                    if (i != s0_idx && i != s1_idx) data_vars.push_back(vars[i]);
                }

                std::vector<std::string> selected_data(4);
                bool valid_mux = true;
                bool is_inv = false;

                for (size_t sel_idx = 0; sel_idx < 4; ++sel_idx) {
                    bool val_s0 = (sel_idx & 1) != 0;
                    bool val_s1 = ((sel_idx >> 1) & 1) != 0;

                    std::string matched_data;
                    for (const auto& dv : data_vars) {
                        bool matches_dv = true;
                        bool matches_not_dv = true;
                        for (bool val_dv : {false, true}) {
                            std::unordered_map<std::string, bool> env;
                            for (const auto& o : data_vars) env[o] = false;
                            env[s0] = val_s0;
                            env[s1] = val_s1;
                            env[dv] = val_dv;
                            bool out_val = expr->evaluate(env);
                            if (out_val != val_dv) matches_dv = false;
                            if (out_val != !val_dv) matches_not_dv = false;
                        }
                        if (matches_dv) {
                            matched_data = dv;
                            break;
                        } else if (matches_not_dv) {
                            matched_data = dv;
                            is_inv = true;
                            break;
                        }
                    }
                    if (matched_data.empty()) {
                        valid_mux = false;
                        break;
                    }
                    selected_data[sel_idx] = matched_data;
                }

                if (valid_mux) {
                    std::set<std::string> distinct_data(selected_data.begin(), selected_data.end());
                    if (distinct_data.size() == 4) {
                        result.gate_type = GateType::MUX;
                        result.is_inverting = is_inv;
                        if (is_inv) result.bubble_pins.push_back(out_pin->name);
                        result.pin_roles[s0] = PinRole::SELECT;
                        result.pin_roles[s1] = PinRole::SELECT;
                        for (const auto& dv : data_vars) result.pin_roles[dv] = PinRole::DATA;
                        result.description = is_inv ? "Inverting 4:1 Multiplexer" : "4:1 Multiplexer";
                        return result;
                    }
                }
            }
        }
    }

    result.gate_type = GateType::MODULE_BOX;
    return result;
}

bool SymbolClassifier::is_sequential(const std::string& cell_type) {
    auto sym = classify(cell_type);
    return sym.gate_type == GateType::DFF || sym.gate_type == GateType::SDFF || sym.gate_type == GateType::LATCH;
}

bool SymbolClassifier::is_output_pin(const std::string& cell_type, const std::string& pin_name) {
    auto sym = classify(cell_type);
    auto it = sym.pin_roles.find(pin_name);
    if (it != sym.pin_roles.end()) {
        return it->second == PinRole::OUTPUT || it->second == PinRole::Q || it->second == PinRole::QN;
    }
    std::string up = pin_name;
    std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    for (const auto& [p, role] : sym.pin_roles) {
        std::string p_up = p;
        std::transform(p_up.begin(), p_up.end(), p_up.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (p_up == up) {
            return role == PinRole::OUTPUT || role == PinRole::Q || role == PinRole::QN;
        }
    }
    return false;
}

bool SymbolClassifier::is_input_pin(const std::string& cell_type, const std::string& pin_name) {
    return !is_output_pin(cell_type, pin_name);
}

} // namespace vajra::gui
