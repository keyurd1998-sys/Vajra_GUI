#pragma once

#include <string>
#include <string_view>

namespace vajra::liberty {

enum class TokenType {
    IDENT,
    STRING,
    NUMBER,
    LBRACE,   // {
    RBRACE,   // }
    LPAREN,   // (
    RPAREN,   // )
    COLON,    // :
    SEMI,     // ;
    COMMA,    // ,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type{TokenType::UNKNOWN};
    std::string_view text;
    int line{1};
    int col{1};

    std::string to_string() const;
};

class Lexer {
public:
    explicit Lexer(std::string_view source);

    Token next_token();
    Token peek_token();

    bool has_more() const;
    int current_line() const { return line_; }
    int current_col() const { return col_; }

    // Strips enclosing quotes and resolves \" and multiline \ line continuations
    static std::string unescape_string(std::string_view s);

private:
    void skip_whitespace_and_comments();
    char peek_char() const;
    char get_char();

    std::string_view source_;
    size_t pos_{0};
    int line_{1};
    int col_{1};

    bool has_peeked_{false};
    Token peeked_token_{};
};

} // namespace vajra::liberty
