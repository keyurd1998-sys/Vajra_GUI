#pragma once

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vajra::liberty {

enum class BoolOp {
    VAR,
    CONST,
    NOT,
    AND,
    OR,
    XOR
};

class BoolExpr {
public:
    explicit BoolExpr(std::string var_name)
        : op_(BoolOp::VAR), var_name_(std::move(var_name)) {}

    explicit BoolExpr(bool const_val)
        : op_(BoolOp::CONST), const_val_(const_val) {}

    BoolExpr(BoolOp op, std::vector<std::shared_ptr<BoolExpr>> children)
        : op_(op), children_(std::move(children)) {}

    BoolOp op() const { return op_; }
    const std::string& var_name() const { return var_name_; }
    bool const_val() const { return const_val_; }
    const std::vector<std::shared_ptr<BoolExpr>>& children() const { return children_; }

    // Evaluates expression under given variable assignment
    bool evaluate(const std::unordered_map<std::string, bool>& env) const;

    // Collects all unique variable names in alphabetical order
    void collect_variables(std::set<std::string>& vars) const;
    std::vector<std::string> get_variables() const;

    // Computes full 2^N truth table across ordered variables
    std::vector<bool> truth_table(const std::vector<std::string>& var_order) const;

    std::string to_string() const;

private:
    BoolOp op_{BoolOp::CONST};
    std::string var_name_;
    bool const_val_{false};
    std::vector<std::shared_ptr<BoolExpr>> children_;
};

class BoolExprParser {
public:
    // Parses Liberty boolean pin function expression (e.g. "(A & B) | !C", "A B + C'", "A ^ B")
    static std::shared_ptr<BoolExpr> parse(std::string_view expr_str);
};

} // namespace vajra::liberty
