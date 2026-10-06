#include "vajra/liberty/bool_expr.hpp"

#include <cctype>
#include <stdexcept>

namespace vajra::liberty {

bool BoolExpr::evaluate(const std::unordered_map<std::string, bool>& env) const {
    switch (op_) {
        case BoolOp::CONST:
            return const_val_;
        case BoolOp::VAR: {
            auto it = env.find(var_name_);
            return (it != env.end()) ? it->second : false;
        }
        case BoolOp::NOT:
            return children_.empty() ? false : !children_[0]->evaluate(env);
        case BoolOp::AND:
            for (const auto& c : children_) {
                if (!c->evaluate(env)) return false;
            }
            return true;
        case BoolOp::OR:
            for (const auto& c : children_) {
                if (c->evaluate(env)) return true;
            }
            return false;
        case BoolOp::XOR: {
            bool res = false;
            for (const auto& c : children_) {
                res ^= c->evaluate(env);
            }
            return res;
        }
    }
    return false;
}

void BoolExpr::collect_variables(std::set<std::string>& vars) const {
    if (op_ == BoolOp::VAR) {
        vars.insert(var_name_);
    } else {
        for (const auto& c : children_) {
            c->collect_variables(vars);
        }
    }
}

std::vector<std::string> BoolExpr::get_variables() const {
    std::set<std::string> vars;
    collect_variables(vars);
    return std::vector<std::string>(vars.begin(), vars.end());
}

std::vector<bool> BoolExpr::truth_table(const std::vector<std::string>& var_order) const {
    size_t n = var_order.size();
    size_t total_rows = (n >= 64) ? 0 : (size_t(1) << n);
    std::vector<bool> tt;
    tt.reserve(total_rows);

    std::unordered_map<std::string, bool> env;
    for (size_t r = 0; r < total_rows; ++r) {
        for (size_t i = 0; i < n; ++i) {
            bool bit = ((r >> (n - 1 - i)) & 1) != 0;
            env[var_order[i]] = bit;
        }
        tt.push_back(evaluate(env));
    }
    return tt;
}

std::string BoolExpr::to_string() const {
    switch (op_) {
        case BoolOp::CONST:
            return const_val_ ? "1" : "0";
        case BoolOp::VAR:
            return var_name_;
        case BoolOp::NOT:
            return "(!" + (children_.empty() ? "" : children_[0]->to_string()) + ")";
        case BoolOp::AND: {
            std::string s = "(";
            for (size_t i = 0; i < children_.size(); ++i) {
                if (i > 0) s += " & ";
                s += children_[i]->to_string();
            }
            return s + ")";
        }
        case BoolOp::OR: {
            std::string s = "(";
            for (size_t i = 0; i < children_.size(); ++i) {
                if (i > 0) s += " | ";
                s += children_[i]->to_string();
            }
            return s + ")";
        }
        case BoolOp::XOR: {
            std::string s = "(";
            for (size_t i = 0; i < children_.size(); ++i) {
                if (i > 0) s += " ^ ";
                s += children_[i]->to_string();
            }
            return s + ")";
        }
    }
    return "";
}

namespace {

enum class ExprTokenType {
    VAR,
    CONST,
    NOT,
    PRIME,
    AND,
    OR,
    XOR,
    LPAREN,
    RPAREN,
    END
};

struct ExprToken {
    ExprTokenType type{ExprTokenType::END};
    std::string text;
};

class ExprLexer {
public:
    explicit ExprLexer(std::string_view src) : src_(src), pos_(0) {}

    ExprToken peek() {
        if (!has_peek_) {
            peek_tok_ = get_next();
            has_peek_ = true;
        }
        return peek_tok_;
    }

    ExprToken next() {
        if (has_peek_) {
            has_peek_ = false;
            return peek_tok_;
        }
        return get_next();
    }

private:
    ExprToken get_next() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
        if (pos_ >= src_.size()) {
            return ExprToken{ExprTokenType::END, ""};
        }

        char c = src_[pos_++];
        switch (c) {
            case '!':
            case '~':
                return ExprToken{ExprTokenType::NOT, std::string(1, c)};
            case '\'':
                return ExprToken{ExprTokenType::PRIME, "'"};
            case '&':
            case '*':
                return ExprToken{ExprTokenType::AND, std::string(1, c)};
            case '|':
            case '+':
                return ExprToken{ExprTokenType::OR, std::string(1, c)};
            case '^':
                return ExprToken{ExprTokenType::XOR, "^"};
            case '(':
                return ExprToken{ExprTokenType::LPAREN, "("};
            case ')':
                return ExprToken{ExprTokenType::RPAREN, ")"};
            case '0':
            case '1':
                // Check if part of identifier or standalone constant
                if (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
                    // Identifier starting with digit, consume rest
                    std::string id(1, c);
                    while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_' || src_[pos_] == '[' || src_[pos_] == ']')) {
                        id += src_[pos_++];
                    }
                    return ExprToken{ExprTokenType::VAR, id};
                }
                return ExprToken{ExprTokenType::CONST, std::string(1, c)};
            default:
                break;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '\\') {
            std::string id(1, c);
            while (pos_ < src_.size()) {
                char ic = src_[pos_];
                if (std::isalnum(static_cast<unsigned char>(ic)) || ic == '_' || ic == '[' || ic == ']' || ic == '.') {
                    id += src_[pos_++];
                } else {
                    break;
                }
            }
            return ExprToken{ExprTokenType::VAR, id};
        }

        return ExprToken{ExprTokenType::END, ""};
    }

    std::string_view src_;
    size_t pos_{0};
    bool has_peek_{false};
    ExprToken peek_tok_{};
};

class ExprParserImpl {
public:
    explicit ExprParserImpl(std::string_view src) : lexer_(src) {}

    std::shared_ptr<BoolExpr> parse() {
        auto expr = parse_or();
        return expr;
    }

private:
    std::shared_ptr<BoolExpr> parse_or() {
        auto node = parse_xor();
        while (lexer_.peek().type == ExprTokenType::OR) {
            lexer_.next();
            auto rhs = parse_xor();
            node = std::make_shared<BoolExpr>(BoolOp::OR, std::vector<std::shared_ptr<BoolExpr>>{node, rhs});
        }
        return node;
    }

    std::shared_ptr<BoolExpr> parse_xor() {
        auto node = parse_and();
        while (lexer_.peek().type == ExprTokenType::XOR) {
            lexer_.next();
            auto rhs = parse_and();
            node = std::make_shared<BoolExpr>(BoolOp::XOR, std::vector<std::shared_ptr<BoolExpr>>{node, rhs});
        }
        return node;
    }

    std::shared_ptr<BoolExpr> parse_and() {
        auto node = parse_unary();
        while (true) {
            auto peek = lexer_.peek();
            if (peek.type == ExprTokenType::AND) {
                lexer_.next();
                auto rhs = parse_unary();
                node = std::make_shared<BoolExpr>(BoolOp::AND, std::vector<std::shared_ptr<BoolExpr>>{node, rhs});
            } else if (peek.type == ExprTokenType::VAR || peek.type == ExprTokenType::CONST ||
                       peek.type == ExprTokenType::LPAREN || peek.type == ExprTokenType::NOT) {
                // Implicit AND
                auto rhs = parse_unary();
                node = std::make_shared<BoolExpr>(BoolOp::AND, std::vector<std::shared_ptr<BoolExpr>>{node, rhs});
            } else {
                break;
            }
        }
        return node;
    }

    std::shared_ptr<BoolExpr> parse_unary() {
        if (lexer_.peek().type == ExprTokenType::NOT) {
            lexer_.next();
            auto sub = parse_unary();
            return std::make_shared<BoolExpr>(BoolOp::NOT, std::vector<std::shared_ptr<BoolExpr>>{sub});
        }
        return parse_postfix();
    }

    std::shared_ptr<BoolExpr> parse_postfix() {
        auto node = parse_primary();
        while (lexer_.peek().type == ExprTokenType::PRIME) {
            lexer_.next();
            node = std::make_shared<BoolExpr>(BoolOp::NOT, std::vector<std::shared_ptr<BoolExpr>>{node});
        }
        return node;
    }

    std::shared_ptr<BoolExpr> parse_primary() {
        auto tok = lexer_.next();
        if (tok.type == ExprTokenType::LPAREN) {
            auto sub = parse_or();
            if (lexer_.peek().type == ExprTokenType::RPAREN) {
                lexer_.next();
            }
            return sub;
        }
        if (tok.type == ExprTokenType::CONST) {
            return std::make_shared<BoolExpr>(tok.text == "1");
        }
        if (tok.type == ExprTokenType::VAR) {
            return std::make_shared<BoolExpr>(tok.text);
        }
        return std::make_shared<BoolExpr>(false);
    }

    ExprLexer lexer_;
};

} // namespace

std::shared_ptr<BoolExpr> BoolExprParser::parse(std::string_view expr_str) {
    ExprParserImpl parser(expr_str);
    return parser.parse();
}

} // namespace vajra::liberty
