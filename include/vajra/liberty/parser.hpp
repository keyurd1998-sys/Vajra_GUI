#pragma once

#include "vajra/liberty/models.hpp"
#include "vajra/liberty/lexer.hpp"
#include "vajra/liberty/loader.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace vajra::liberty {

class Parser {
public:
    Parser() = default;

    // Parse from in-memory string buffer
    Library parse_string(std::string_view content, const std::string& filename = "<memory>");

    // Parse from file (.lib or gzip-compressed .lib.gz)
    Library parse_file(const std::string& filepath);

    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    void parse_library_body(Lexer& lexer, Library& lib);
    void parse_lu_table_template(Lexer& lexer, Library& lib, const std::string& tmpl_name);
    void parse_cell(Lexer& lexer, Library& lib, const std::string& cell_name);
    void parse_pin(Lexer& lexer, Cell& cell, const std::string& pin_name);
    void parse_bus(Lexer& lexer, Cell& cell, const std::string& bus_name);
    void parse_timing(Lexer& lexer, Pin& pin, const std::string& arg);
    LUT2D parse_lut2d(Lexer& lexer, const std::string& tmpl_name);
    void parse_ff(Lexer& lexer, std::vector<SequentialBlock>& seq_blocks, const std::vector<std::string>& args, bool is_latch);
    void parse_test_cell(Lexer& lexer, Cell& cell);
    void skip_group_body(Lexer& lexer);
    std::vector<std::string> parse_paren_args(Lexer& lexer);

    std::vector<double> parse_double_list(const std::vector<std::string>& raw_args);

    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};

} // namespace vajra::liberty
