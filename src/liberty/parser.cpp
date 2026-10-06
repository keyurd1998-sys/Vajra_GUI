#include "vajra/liberty/parser.hpp"
#include "vajra/liberty/template_resolver.hpp"
#include "vajra/liberty/normalizer.hpp"

#include <iostream>
#include <sstream>

namespace vajra::liberty {

Library Parser::parse_file(const std::string& filepath) {
    std::string content = Loader::load_file(filepath);
    return parse_string(content, filepath);
}

std::vector<std::string> Parser::parse_paren_args(Lexer& lexer) {
    std::vector<std::string> args;
    if (lexer.peek_token().type != TokenType::LPAREN) {
        return args;
    }
    lexer.next_token(); // consume '('

    std::string current_arg;
    bool in_arg = false;

    while (lexer.peek_token().type != TokenType::RPAREN && lexer.peek_token().type != TokenType::END_OF_FILE) {
        if (lexer.peek_token().type == TokenType::COMMA) {
            lexer.next_token();
            if (in_arg) {
                args.push_back(current_arg);
                current_arg.clear();
                in_arg = false;
            }
            continue;
        }

        Token t = lexer.next_token();
        std::string tok_str;
        if (t.type == TokenType::STRING) {
            tok_str = Lexer::unescape_string(t.text);
        } else {
            tok_str = std::string(t.text);
        }

        if (!in_arg) {
            current_arg = tok_str;
            in_arg = true;
        } else {
            char last = current_arg.empty() ? '\0' : current_arg.back();
            char first = tok_str.empty() ? '\0' : tok_str.front();
            if (last == ':' || last == '[' || first == ':' || first == ']' || first == '[') {
                current_arg += tok_str;
            } else {
                current_arg += " " + tok_str;
            }
        }
    }

    if (in_arg) {
        args.push_back(current_arg);
    }

    if (lexer.peek_token().type == TokenType::RPAREN) {
        lexer.next_token(); // consume ')'
    }
    return args;
}

std::vector<double> Parser::parse_double_list(const std::vector<std::string>& raw_args) {
    std::vector<double> result;
    for (const auto& raw : raw_args) {
        std::stringstream ss(raw);
        std::string item;
        while (std::getline(ss, item, ',')) {
            // Trim whitespace
            size_t start = item.find_first_not_of(" \t\r\n\"");
            size_t end = item.find_last_not_of(" \t\r\n\"");
            if (start != std::string::npos && end != std::string::npos) {
                std::string num_str = item.substr(start, end - start + 1);
                try {
                    result.push_back(std::stod(num_str));
                } catch (...) {
                    // Ignore non-numeric tokens
                }
            }
        }
    }
    return result;
}

void Parser::skip_group_body(Lexer& lexer) {
    int depth = 1;
    while (depth > 0 && lexer.has_more()) {
        Token t = lexer.next_token();
        if (t.type == TokenType::LBRACE) {
            depth++;
        } else if (t.type == TokenType::RBRACE) {
            depth--;
        } else if (t.type == TokenType::END_OF_FILE) {
            break;
        }
    }
}

LUT2D Parser::parse_lut2d(Lexer& lexer, const std::string& tmpl_name) {
    LUT2D lut;
    lut.template_name = tmpl_name;

    if (lexer.peek_token().type != TokenType::LBRACE) return lut;
    lexer.next_token(); // consume '{'

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token(); // consume ';'
            }
            if (t.text == "index_1") {
                lut.index_1 = parse_double_list(args);
            } else if (t.text == "index_2") {
                lut.index_2 = parse_double_list(args);
            } else if (t.text == "values") {
                lut.values = parse_double_list(args);
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "variable_1") lut.variable_1 = val;
            else if (t.text == "variable_2") lut.variable_2 = val;
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    return lut;
}

void Parser::parse_lu_table_template(Lexer& lexer, Library& lib, const std::string& tmpl_name) {
    TableTemplate tmpl;
    tmpl.name = tmpl_name;

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
            if (t.text == "index_1") {
                tmpl.index_1 = parse_double_list(args);
            } else if (t.text == "index_2") {
                tmpl.index_2 = parse_double_list(args);
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "variable_1") tmpl.variable_1 = val;
            else if (t.text == "variable_2") tmpl.variable_2 = val;
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    lib.templates[tmpl.name] = std::move(tmpl);
}

void Parser::parse_timing(Lexer& lexer, Pin& pin, const std::string& arg) {
    TimingArc arc;
    if (!arg.empty()) {
        arc.related_pin = arg;
    }

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            std::string sub_tmpl = args.empty() ? "" : args[0];

            if (lexer.peek_token().type == TokenType::LBRACE) {
                if (t.text == "cell_rise") {
                    arc.cell_rise = parse_lut2d(lexer, sub_tmpl);
                } else if (t.text == "cell_fall") {
                    arc.cell_fall = parse_lut2d(lexer, sub_tmpl);
                } else if (t.text == "rise_transition") {
                    arc.rise_transition = parse_lut2d(lexer, sub_tmpl);
                } else if (t.text == "fall_transition") {
                    arc.fall_transition = parse_lut2d(lexer, sub_tmpl);
                } else if (t.text == "rise_constraint") {
                    arc.rise_constraint = parse_lut2d(lexer, sub_tmpl);
                } else if (t.text == "fall_constraint") {
                    arc.fall_constraint = parse_lut2d(lexer, sub_tmpl);
                } else {
                    lexer.next_token(); // consume '{'
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "related_pin") {
                arc.related_pin = val;
            } else if (t.text == "timing_type") {
                arc.timing_type = TimingArc::string_to_timing_type(val);
            } else if (t.text == "timing_sense") {
                arc.timing_sense = TimingArc::string_to_timing_sense(val);
            } else if (t.text == "when") {
                arc.when = val;
            }
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    pin.timing_arcs.push_back(std::move(arc));
}

void Parser::parse_pin(Lexer& lexer, Cell& cell, const std::string& pin_name) {
    Pin pin;
    pin.name = pin_name;

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                if (t.text == "timing") {
                    std::string rel_arg = args.empty() ? "" : args[0];
                    parse_timing(lexer, pin, rel_arg);
                } else {
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            try {
                if (t.text == "direction") {
                    pin.direction = Pin::string_to_direction(val);
                } else if (t.text == "capacitance") {
                    pin.capacitance = std::stod(val);
                } else if (t.text == "rise_capacitance") {
                    pin.rise_capacitance = std::stod(val);
                } else if (t.text == "fall_capacitance") {
                    pin.fall_capacitance = std::stod(val);
                } else if (t.text == "max_capacitance") {
                    pin.max_capacitance = std::stod(val);
                } else if (t.text == "min_capacitance") {
                    pin.min_capacitance = std::stod(val);
                } else if (t.text == "max_transition") {
                    pin.max_transition = std::stod(val);
                } else if (t.text == "clock" || t.text == "is_clock") {
                    pin.is_clock = (val == "true" || val == "1");
                } else if (t.text == "function") {
                    pin.function_str = val;
                } else if (t.text == "signal_type") {
                    std::string st = val;
                    for (char& c : st) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (st.find("test_scan") != std::string::npos && !cell.test_cell) {
                        cell.test_cell = TestCell();
                    }
                }
            } catch (...) {}
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token(); // consume '{'
            if (t.text == "timing") {
                parse_timing(lexer, pin, "");
            } else {
                skip_group_body(lexer);
            }
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    cell.pins[pin.name] = std::move(pin);
}

void Parser::parse_bus(Lexer& lexer, Cell& cell, const std::string& bus_name) {
    Pin bus_pin;
    bus_pin.name = bus_name;
    std::vector<std::string> child_pin_names;

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                if (t.text == "pin") {
                    std::string pname = args.empty() ? "" : args[0];
                    Pin child_pin;
                    child_pin.name = pname;
                    child_pin.direction = bus_pin.direction;
                    child_pin.capacitance = bus_pin.capacitance;

                    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
                        Token pt = lexer.next_token();
                        if (pt.type != TokenType::IDENT) continue;
                        if (lexer.peek_token().type == TokenType::LPAREN) {
                            auto pargs = parse_paren_args(lexer);
                            if (lexer.peek_token().type == TokenType::LBRACE) {
                                lexer.next_token();
                                if (pt.text == "timing") {
                                    parse_timing(lexer, child_pin, pargs.empty() ? "" : pargs[0]);
                                } else {
                                    skip_group_body(lexer);
                                }
                            } else if (lexer.peek_token().type == TokenType::SEMI) {
                                lexer.next_token();
                            }
                        } else if (lexer.peek_token().type == TokenType::COLON) {
                            lexer.next_token();
                            std::string val;
                            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                                Token vt = lexer.next_token();
                                if (!val.empty()) val += " ";
                                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                                else val += std::string(vt.text);
                            }
                            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();
                            try {
                                if (pt.text == "direction") child_pin.direction = Pin::string_to_direction(val);
                                else if (pt.text == "capacitance") child_pin.capacitance = std::stod(val);
                                else if (pt.text == "rise_capacitance") child_pin.rise_capacitance = std::stod(val);
                                else if (pt.text == "fall_capacitance") child_pin.fall_capacitance = std::stod(val);
                                else if (pt.text == "max_capacitance") child_pin.max_capacitance = std::stod(val);
                                else if (pt.text == "min_capacitance") child_pin.min_capacitance = std::stod(val);
                                else if (pt.text == "max_transition") child_pin.max_transition = std::stod(val);
                                else if (pt.text == "clock" || pt.text == "is_clock") child_pin.is_clock = (val == "true" || val == "1");
                                else if (pt.text == "function") child_pin.function_str = val;
                            } catch (...) {}
                        } else if (lexer.peek_token().type == TokenType::LBRACE) {
                            lexer.next_token();
                            if (pt.text == "timing") {
                                parse_timing(lexer, child_pin, "");
                            } else {
                                skip_group_body(lexer);
                            }
                        }
                    }
                    if (lexer.peek_token().type == TokenType::RBRACE) lexer.next_token();
                    child_pin_names.push_back(child_pin.name);
                    cell.pins[child_pin.name] = std::move(child_pin);
                } else if (t.text == "timing") {
                    parse_timing(lexer, bus_pin, args.empty() ? "" : args[0]);
                } else {
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token();
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();
            try {
                if (t.text == "direction") bus_pin.direction = Pin::string_to_direction(val);
                else if (t.text == "capacitance") bus_pin.capacitance = std::stod(val);
                else if (t.text == "rise_capacitance") bus_pin.rise_capacitance = std::stod(val);
                else if (t.text == "fall_capacitance") bus_pin.fall_capacitance = std::stod(val);
                else if (t.text == "max_capacitance") bus_pin.max_capacitance = std::stod(val);
                else if (t.text == "min_capacitance") bus_pin.min_capacitance = std::stod(val);
                else if (t.text == "max_transition") bus_pin.max_transition = std::stod(val);
                else if (t.text == "clock" || t.text == "is_clock") bus_pin.is_clock = (val == "true" || val == "1");
            } catch (...) {}
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }
    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token();
    }

    // Synchronize direction and capacitance between bus and child pins
    for (const auto& cp_name : child_pin_names) {
        auto it = cell.pins.find(cp_name);
        if (it != cell.pins.end()) {
            if (it->second.direction == PinDirection::UNKNOWN && bus_pin.direction != PinDirection::UNKNOWN) {
                it->second.direction = bus_pin.direction;
            } else if (bus_pin.direction == PinDirection::UNKNOWN && it->second.direction != PinDirection::UNKNOWN) {
                bus_pin.direction = it->second.direction;
            }
            if (it->second.capacitance == 0.0 && bus_pin.capacitance != 0.0) {
                it->second.capacitance = bus_pin.capacitance;
            }
        }
    }

    cell.pins[bus_pin.name] = std::move(bus_pin);
}

void Parser::parse_ff(Lexer& lexer, std::vector<SequentialBlock>& seq_blocks, const std::vector<std::string>& args, bool is_latch) {
    SequentialBlock seq;
    seq.is_latch = is_latch;
    if (args.size() >= 1) seq.var1 = args[0];
    if (args.size() >= 2) seq.var2 = args[1];

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "clocked_on") seq.clocked_on = val;
            else if (t.text == "next_state" || t.text == "data_in") seq.next_state = val;
            else if (t.text == "clear") seq.clear = val;
            else if (t.text == "preset") seq.preset = val;
            else if (t.text == "clear_preset_var1") seq.clear_preset_var1 = val;
            else if (t.text == "clear_preset_var2") seq.clear_preset_var2 = val;
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        } else if (lexer.peek_token().type == TokenType::LPAREN) {
            parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();
            else if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token();
                skip_group_body(lexer);
            }
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    seq_blocks.push_back(std::move(seq));
}

void Parser::parse_test_cell(Lexer& lexer, Cell& cell) {
    TestCell tc;

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                if (t.text == "ff") {
                    parse_ff(lexer, tc.sequential_blocks, args, false);
                } else if (t.text == "pin") {
                    std::string pname = args.empty() ? "" : args[0];
                    // Inspect pin attributes inside test_cell for scan roles
                    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
                        Token pt = lexer.next_token();
                        if (pt.type == TokenType::IDENT && lexer.peek_token().type == TokenType::COLON) {
                            lexer.next_token(); // consume ':'
                            std::string val;
                            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                                Token vt = lexer.next_token();
                                if (!val.empty()) val += " ";
                                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                                else val += std::string(vt.text);
                            }
                            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

                            if (pt.text == "signal_type") {
                                if (val == "test_scan_in") tc.scan_in = pname;
                                else if (val == "test_scan_enable") tc.scan_enable = pname;
                                else if (val == "test_scan_out") tc.scan_out = pname;
                            }
                        }
                    }
                    if (lexer.peek_token().type == TokenType::RBRACE) lexer.next_token();
                } else {
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "scan_in") tc.scan_in = val;
            else if (t.text == "scan_enable") tc.scan_enable = val;
            else if (t.text == "scan_out") tc.scan_out = val;
            else if (t.text == "data_in") tc.data_in = val;
            else if (t.text == "clock") tc.clock = val;
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
    cell.test_cell = std::move(tc);
}

void Parser::parse_cell(Lexer& lexer, Library& lib, const std::string& cell_name) {
    Cell cell;
    cell.name = cell_name;

    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                if (t.text == "pin") {
                    std::string pname = args.empty() ? "" : args[0];
                    parse_pin(lexer, cell, pname);
                } else if (t.text == "bus") {
                    std::string bname = args.empty() ? "" : args[0];
                    parse_bus(lexer, cell, bname);
                } else if (t.text == "ff") {
                    parse_ff(lexer, cell.sequential_blocks, args, false);
                } else if (t.text == "latch") {
                    parse_ff(lexer, cell.sequential_blocks, args, true);
                } else if (t.text == "test_cell") {
                    parse_test_cell(lexer, cell);
                } else {
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token();
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            try {
                if (t.text == "area") {
                    cell.area = std::stod(val);
                } else if (t.text == "cell_footprint") {
                    cell.cell_footprint = val;
                } else if (t.text == "leakage_power" || t.text == "cell_leakage_power") {
                    cell.leakage_power = std::stod(val);
                } else if (t.text == "dont_touch") {
                    cell.dont_touch = (val == "true" || val == "1");
                } else if (t.text == "dont_use") {
                    cell.dont_use = (val == "true" || val == "1");
                } else if (t.text == "is_macro") {
                    cell.is_macro = (val == "true" || val == "1");
                } else if (t.text == "is_pad") {
                    cell.is_pad = (val == "true" || val == "1");
                } else if (t.text == "is_clock_gating") {
                    cell.is_clock_gating = (val == "true" || val == "1");
                } else if (t.text == "is_tie") {
                    cell.is_tie = (val == "true" || val == "1");
                } else if (t.text == "clock_gating_integrated_cell") {
                    cell.is_clock_gating = true;
                    cell.attributes[std::string(t.text)] = val;
                } else {
                    cell.attributes[std::string(t.text)] = val;
                }
            } catch (...) {}
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token(); // consume '{'
            if (t.text == "test_cell") {
                parse_test_cell(lexer, cell);
            } else {
                skip_group_body(lexer);
            }
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }

    if (cell.test_cell) {
        for (const auto& [pname, pin] : cell.pins) {
            std::string pu = pname;
            for (char& c : pu) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (cell.test_cell->scan_in.empty() && (pu == "SI" || pu == "SCD" || pu == "TI")) {
                cell.test_cell->scan_in = pname;
            } else if (cell.test_cell->scan_enable.empty() && (pu == "SE" || pu == "SCE" || pu == "TE")) {
                cell.test_cell->scan_enable = pname;
            } else if (cell.test_cell->scan_out.empty() && (pu == "SO")) {
                cell.test_cell->scan_out = pname;
            } else if (cell.test_cell->data_in.empty() && (pu == "D" || pu == "DATA")) {
                cell.test_cell->data_in = pname;
            } else if (cell.test_cell->clock.empty() && (pin.is_clock || pu == "CLK" || pu == "CK")) {
                cell.test_cell->clock = pname;
            }
        }
    }
    cell.cell_class = cell.infer_classification();

    lib.cells[cell.name] = std::move(cell);
}

void Parser::parse_library_body(Lexer& lexer, Library& lib) {
    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
        Token t = lexer.next_token();
        if (t.type != TokenType::IDENT) continue;

        if (lexer.peek_token().type == TokenType::LPAREN) {
            auto args = parse_paren_args(lexer);
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                if (t.text == "lu_table_template") {
                    std::string tmpl_name = args.empty() ? "" : args[0];
                    parse_lu_table_template(lexer, lib, tmpl_name);
                } else if (t.text == "cell") {
                    std::string cell_name = args.empty() ? "" : args[0];
                    parse_cell(lexer, lib, cell_name);
                } else if (t.text == "operating_conditions") {
                    OperatingCondition cond;
                    cond.name = args.empty() ? "" : args[0];
                    while (lexer.peek_token().type != TokenType::RBRACE && lexer.peek_token().type != TokenType::END_OF_FILE) {
                        Token ct = lexer.next_token();
                        if (ct.type == TokenType::IDENT && lexer.peek_token().type == TokenType::COLON) {
                            lexer.next_token();
                            std::string val;
                            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                                Token vt = lexer.next_token();
                                if (!val.empty()) val += " ";
                                val += std::string(vt.text);
                            }
                            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();
                            try {
                                if (ct.text == "process") cond.process = std::stod(val);
                                else if (ct.text == "voltage") cond.voltage = std::stod(val);
                                else if (ct.text == "temperature") cond.temperature = std::stod(val);
                            } catch (...) {}
                        }
                    }
                    if (lexer.peek_token().type == TokenType::RBRACE) lexer.next_token();
                    lib.op_conds[cond.name] = cond;
                } else {
                    skip_group_body(lexer);
                }
            } else if (lexer.peek_token().type == TokenType::SEMI) {
                lexer.next_token(); // consume ';'
                if (t.text == "capacitive_load_unit") {
                    if (args.size() >= 2) {
                        try {
                            lib.units.set_capacitive_load_unit(std::stod(args[0]), args[1]);
                        } catch (...) {}
                    }
                } else {
                    std::string val;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0) val += ", ";
                        val += args[i];
                    }
                    lib.attributes[std::string(t.text)] = val;
                }
            }
        } else if (lexer.peek_token().type == TokenType::COLON) {
            lexer.next_token(); // consume ':'
            std::string val;
            while (lexer.peek_token().type != TokenType::SEMI && lexer.peek_token().type != TokenType::END_OF_FILE) {
                Token vt = lexer.next_token();
                if (!val.empty()) val += " ";
                if (vt.type == TokenType::STRING) val += Lexer::unescape_string(vt.text);
                else val += std::string(vt.text);
            }
            if (lexer.peek_token().type == TokenType::SEMI) lexer.next_token();

            if (t.text == "time_unit") {
                lib.units.set_time_unit(val);
            } else if (t.text == "voltage_unit") {
                lib.units.set_voltage_unit(val);
            } else if (t.text == "current_unit") {
                lib.units.set_current_unit(val);
            } else if (t.text == "pulling_resistance_unit") {
                lib.units.set_pulling_resistance_unit(val);
            } else if (t.text == "default_operating_conditions") {
                lib.default_operating_conditions = val;
                lib.attributes[std::string(t.text)] = val;
            } else {
                lib.attributes[std::string(t.text)] = val;
            }
        } else if (lexer.peek_token().type == TokenType::LBRACE) {
            lexer.next_token();
            skip_group_body(lexer);
        }
    }

    if (lexer.peek_token().type == TokenType::RBRACE) {
        lexer.next_token(); // consume '}'
    }
}

Library Parser::parse_string(std::string_view content, const std::string& filename) {
    Lexer lexer(content);
    Library lib;
    lib.filepath = filename;

    while (lexer.has_more()) {
        Token t = lexer.next_token();
        if (t.type == TokenType::END_OF_FILE) break;
        if (t.type != TokenType::IDENT) continue;

        if (t.text == "library") {
            auto args = parse_paren_args(lexer);
            if (!args.empty()) {
                lib.name = args[0];
            }
            if (lexer.peek_token().type == TokenType::LBRACE) {
                lexer.next_token(); // consume '{'
                parse_library_body(lexer, lib);
            }
            break; // Finished library group
        }
    }

    // Semantic Resolution and Normalization
    resolve_templates(lib);
    normalize_units(lib);

    return lib;
}

} // namespace vajra::liberty
