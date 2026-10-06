#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

namespace vajra::liberty {

struct Units {
    // Normalization multipliers to internal standard units:
    // Time -> picoseconds (ps)
    // Capacitance -> femtofarads (fF)
    // Voltage -> millivolts (mV)
    // Current -> microamperes (uA)
    // Resistance -> ohms (ohm)
    // Power -> microwatts (uW)

    double time_scale_ps{1000.0};       // Default: 1ns = 1000ps
    double cap_scale_ff{1000.0};        // Default: 1pF = 1000fF
    double voltage_scale_mv{1000.0};    // Default: 1V = 1000mV
    double current_scale_ua{1.0};       // Default: 1uA = 1uA
    double resistance_scale_ohm{1.0};   // Default: 1ohm = 1ohm
    double power_scale_uw{1.0};         // Default: 1uW = 1uW

    double initial_time_scale_ps{1000.0};
    double initial_cap_scale_ff{1000.0};

    std::string time_unit_str{"1ns"};
    std::string cap_unit_str{"1pf"};
    std::string voltage_unit_str{"1V"};
    std::string current_unit_str{"1uA"};
    std::string resistance_unit_str{"1ohm"};
    std::string power_unit_str{"1uW"};

    const std::string& time_unit() const { return time_unit_str; }
    const std::string& capacitive_load_unit() const { return cap_unit_str; }
    const std::string& voltage_unit() const { return voltage_unit_str; }
    const std::string& current_unit() const { return current_unit_str; }
    const std::string& pulling_resistance_unit() const { return resistance_unit_str; }

    double time_to_ps_factor() const { return (initial_time_scale_ps > 0.0) ? initial_time_scale_ps : 1000.0; }
    double cap_to_ff_factor() const { return (initial_cap_scale_ff > 0.0) ? initial_cap_scale_ff : 1000.0; }

    void set_time_unit(std::string_view val_str) {
        time_unit_str = std::string(val_str);
        std::string s;
        for (char c : val_str) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '"') {
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (s == "1ps") {
            time_scale_ps = 1.0;
        } else if (s == "10ps") {
            time_scale_ps = 10.0;
        } else if (s == "100ps") {
            time_scale_ps = 100.0;
        } else if (s == "1ns") {
            time_scale_ps = 1000.0;
        } else if (s == "10ns") {
            time_scale_ps = 10000.0;
        } else if (s == "1us") {
            time_scale_ps = 1000000.0;
        } else {
            // Try parsing numeric prefix
            size_t idx = 0;
            try {
                double num = std::stod(s, &idx);
                std::string suffix = s.substr(idx);
                if (suffix == "ps") time_scale_ps = num;
                else if (suffix == "ns") time_scale_ps = num * 1000.0;
                else if (suffix == "us") time_scale_ps = num * 1000000.0;
            } catch (...) {
                time_scale_ps = 1000.0;
            }
        }
        initial_time_scale_ps = time_scale_ps;
    }

    void set_capacitive_load_unit(double val, std::string_view unit_name) {
        std::string u;
        for (char c : unit_name) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '"') {
                u += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        cap_unit_str = std::to_string(val) + u;
        if (u == "pf") {
            cap_scale_ff = val * 1000.0;
        } else if (u == "ff") {
            cap_scale_ff = val;
        } else if (u == "uf") {
            cap_scale_ff = val * 1e9;
        } else {
            cap_scale_ff = val * 1000.0;
        }
        initial_cap_scale_ff = cap_scale_ff;
    }

    void set_voltage_unit(std::string_view val_str) {
        voltage_unit_str = std::string(val_str);
        std::string s;
        for (char c : val_str) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '"') {
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (s == "1v" || s == "v") {
            voltage_scale_mv = 1000.0;
        } else if (s == "1mv" || s == "mv") {
            voltage_scale_mv = 1.0;
        } else {
            size_t idx = 0;
            try {
                double num = std::stod(s, &idx);
                std::string suffix = s.substr(idx);
                if (suffix == "v") voltage_scale_mv = num * 1000.0;
                else if (suffix == "mv") voltage_scale_mv = num;
            } catch (...) {
                voltage_scale_mv = 1000.0;
            }
        }
    }

    void set_current_unit(std::string_view val_str) {
        current_unit_str = std::string(val_str);
        std::string s;
        for (char c : val_str) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '"') {
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (s == "1ma" || s == "ma") {
            current_scale_ua = 1000.0;
        } else if (s == "1ua" || s == "ua") {
            current_scale_ua = 1.0;
        } else if (s == "1na" || s == "na") {
            current_scale_ua = 0.001;
        }
    }

    void set_pulling_resistance_unit(std::string_view val_str) {
        resistance_unit_str = std::string(val_str);
        std::string s;
        for (char c : val_str) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '"') {
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (s == "1kohm" || s == "kohm") {
            resistance_scale_ohm = 1000.0;
        } else if (s == "1ohm" || s == "ohm") {
            resistance_scale_ohm = 1.0;
        } else if (s == "1mohm" || s == "mohm") {
            resistance_scale_ohm = 1e6;
        }
    }
};

} // namespace vajra::liberty
