#include "vajra/liberty/lexer.hpp"

#include <cctype>

namespace vajra::liberty {

std::string Token::to_string() const {
    switch (type) {
        case TokenType::IDENT: return "IDENT(" + std::string(text) + ")";
        case TokenType::STRING: return "STRING(" + std::string(text) + ")";
        case TokenType::NUMBER: return "NUMBER(" + std::string(text) + ")";
        case TokenType::LBRACE: return "LBRACE({)";
        case TokenType::RBRACE: return "RBRACE(})";
        case TokenType::LPAREN: return "LPAREN(()";
        case TokenType::RPAREN: return "RPAREN())";
        case TokenType::COLON: return "COLON(:)";
        case TokenType::SEMI: return "SEMI(;)";
        case TokenType::COMMA: return "COMMA(,)";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN(" + std::string(text) + ")";
    }
}

Lexer::Lexer(std::string_view source)
    : source_(source), pos_(0), line_(1), col_(1) {}

char Lexer::peek_char() const {
    if (pos_ < source_.size()) {
        return source_[pos_];
    }
    return '\0';
}

char Lexer::get_char() {
    if (pos_ < source_.size()) {
        char c = source_[pos_++];
        if (c == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
        return c;
    }
    return '\0';
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < source_.size()) {
        char c = source_[pos_];

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            get_char();
            continue;
        }
        if (c == '\n') {
            get_char();
            continue;
        }

        // Line continuation outside string
        if (c == '\\' && pos_ + 1 < source_.size()) {
            char next = source_[pos_ + 1];
            if (next == '\n' || next == '\r') {
                get_char(); // consume '\'
                if (peek_char() == '\r') get_char();
                if (peek_char() == '\n') get_char();
                continue;
            }
        }

        // Comments
        if (c == '/' && pos_ + 1 < source_.size()) {
            char next = source_[pos_ + 1];
            if (next == '/') {
                // Line comment
                get_char(); // '/'
                get_char(); // '/'
                while (pos_ < source_.size() && source_[pos_] != '\n') {
                    get_char();
                }
                continue;
            } else if (next == '*') {
                // Block comment
                get_char(); // '/'
                get_char(); // '*'
                while (pos_ + 1 < source_.size()) {
                    if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
                        get_char(); // '*'
                        get_char(); // '/'
                        break;
                    }
                    get_char();
                }
                continue;
            }
        }

        break;
    }
}

bool Lexer::has_more() const {
    return pos_ < source_.size() || has_peeked_;
}

Token Lexer::peek_token() {
    if (!has_peeked_) {
        peeked_token_ = next_token();
        has_peeked_ = true;
    }
    return peeked_token_;
}

Token Lexer::next_token() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_token_;
    }

    skip_whitespace_and_comments();

    if (pos_ >= source_.size()) {
        return Token{TokenType::END_OF_FILE, "", line_, col_};
    }

    int start_line = line_;
    int start_col = col_;
    size_t start_pos = pos_;
    char c = source_[pos_];

    // Single-character punctuation tokens
    switch (c) {
        case '{': get_char(); return Token{TokenType::LBRACE, source_.substr(start_pos, 1), start_line, start_col};
        case '}': get_char(); return Token{TokenType::RBRACE, source_.substr(start_pos, 1), start_line, start_col};
        case '(': get_char(); return Token{TokenType::LPAREN, source_.substr(start_pos, 1), start_line, start_col};
        case ')': get_char(); return Token{TokenType::RPAREN, source_.substr(start_pos, 1), start_line, start_col};
        case ':': get_char(); return Token{TokenType::COLON,  source_.substr(start_pos, 1), start_line, start_col};
        case ';': get_char(); return Token{TokenType::SEMI,   source_.substr(start_pos, 1), start_line, start_col};
        case ',': get_char(); return Token{TokenType::COMMA,  source_.substr(start_pos, 1), start_line, start_col};
        default: break;
    }

    // Quoted strings
    if (c == '"') {
        get_char(); // opening quote
        while (pos_ < source_.size()) {
            char sc = source_[pos_];
            if (sc == '\\') {
                get_char(); // '\'
                if (pos_ < source_.size()) {
                    get_char(); // escaped char or newline
                }
            } else if (sc == '"') {
                get_char(); // closing quote
                break;
            } else {
                get_char();
            }
        }
        return Token{TokenType::STRING, source_.substr(start_pos, pos_ - start_pos), start_line, start_col};
    }

    // Numbers: optional sign [+-], digits, optional '.', digits, optional [eE][+-]digits
    bool is_sign = (c == '+' || c == '-');
    bool is_digit = std::isdigit(static_cast<unsigned char>(c));
    bool is_dot_num = (c == '.' && pos_ + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])));
    bool sign_followed_by_num = is_sign && (pos_ + 1 < source_.size()) &&
        (std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])) ||
         (source_[pos_ + 1] == '.' && pos_ + 2 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_ + 2]))));

    if (is_digit || is_dot_num || sign_followed_by_num) {
        if (is_sign) get_char();
        while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
            get_char();
        }
        if (pos_ < source_.size() && source_[pos_] == '.') {
            get_char();
            while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
                get_char();
            }
        }
        if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
            get_char();
            if (pos_ < source_.size() && (source_[pos_] == '+' || source_[pos_] == '-')) {
                get_char();
            }
            while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
                get_char();
            }
        }
        return Token{TokenType::NUMBER, source_.substr(start_pos, pos_ - start_pos), start_line, start_col};
    }

    // Identifiers and keyword tokens
    // Can include letters, digits, _, ., -, +, [, ], \escapes
    while (pos_ < source_.size()) {
        char ic = source_[pos_];
        if (ic == '\\' && pos_ + 1 < source_.size()) {
            get_char(); // '\'
            get_char(); // escaped character
            continue;
        }
        if (std::isalnum(static_cast<unsigned char>(ic)) || ic == '_' || ic == '.' ||
            ic == '-' || ic == '+' || ic == '[' || ic == ']' || ic == '\'' || ic == '!') {
            get_char();
        } else {
            break;
        }
    }

    if (pos_ > start_pos) {
        return Token{TokenType::IDENT, source_.substr(start_pos, pos_ - start_pos), start_line, start_col};
    }

    // Unrecognized single character fallback
    get_char();
    return Token{TokenType::UNKNOWN, source_.substr(start_pos, 1), start_line, start_col};
}

std::string Lexer::unescape_string(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
    }
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '\n') {
                i++; // skip newline continuation
                continue;
            } else if (next == '\r') {
                i++;
                if (i + 1 < s.size() && s[i + 1] == '\n') {
                    i++;
                }
                continue;
            } else if (next == '"' || next == '\\') {
                out += next;
                i++;
                continue;
            }
        }
        out += c;
    }
    return out;
}

} // namespace vajra::liberty
