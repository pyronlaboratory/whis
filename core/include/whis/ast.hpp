#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace whis::ast {

using DimArray = std::array<int16_t, 7>;

/**
 * @brief Source location tracking for error reporting and debugging.
 */
struct SourceLocation {
  std::string file;
  uint32_t line = 1;
  uint32_t column = 1;
};

/**
 * @brief Base structure for all nodes to ensure location tracking.
 */
struct Node {
  SourceLocation loc;
};

// =========================================================================
// 1. Expressions (§3.2 and §5 Grammar)
// =========================================================================

struct Expression;

struct Identifier : Node {
  std::string name;
};

/**
 * @brief Represents a physical value with SI dimensions.
 * Dimension indices: [0:M, 1:L, 2:T, 3:I, 4:Θ, 5:N, 6:J]
 */
struct DimensionedValue : Node {
  double value = 0.0;
  DimArray dimensions = {};
  bool suffix_applied = false;
};

struct VectorLiteral : Node {
  std::vector<Expression> elements;
};

struct TweakExpr : Node {
  std::unique_ptr<Expression> initial;
  std::unique_ptr<Expression> min;
  std::unique_ptr<Expression> max;
};

struct CallExpr : Node {
  std::string name;  // Dotted name
  std::vector<Expression> args;
};

enum class BinaryOp { Add, Sub, Mul, Div, Pow, Eq, Ne, Lt, Le, Gt, Ge };

struct BinaryExpr : Node {
  BinaryOp op;
  std::unique_ptr<Expression> left;
  std::unique_ptr<Expression> right;
};

struct UnaryExpr : Node {
  char op;  // Only '-' is supported. Change to std::string if unary 'not'/'!'
            // is added.
  std::unique_ptr<Expression> expr;
};

using ExpressionData = std::variant<Identifier, DimensionedValue, VectorLiteral,
                                    TweakExpr, CallExpr, BinaryExpr, UnaryExpr>;

struct Expression : Node {
  ExpressionData data;
};

// =========================================================================
// 2. Statements (§3.2 and §6 Grammar)
// =========================================================================

struct Property : Node {
  Identifier name;
  Expression value;
};

struct CreateStmt : Node {
  Identifier type;
  std::vector<Property> properties;
};

struct LetStmt : Node {
  Identifier name;
  std::optional<Expression> value;
};

struct EnableStmt : Node {
  bool is_enable;  // true = enable, false = disable
  Identifier name;
  // Arguments can be properties (key: value) or positional expressions
  std::vector<std::variant<Property, Expression>> args;
};

struct ShowStmt : Node {
  bool is_show;  // true = show, false = hide
  Identifier name;
  std::vector<Expression> args;
  std::optional<Identifier> on_target;

  struct Resolution {
    Identifier name;
    std::vector<Identifier> dims;
  };
  std::optional<Resolution> resolution;
};

struct AssignStmt : Node {
  std::string target;  // Dotted name
  Expression value;
};

// --- Control Statements ---

struct CodeBlock;
struct ControlStmt;

struct IfStmt : Node {
  Expression condition;
  std::unique_ptr<CodeBlock> then_block;
  // NOTE: else_branch holds unique_ptr<ControlStmt> rather than
  // unique_ptr<IfStmt>, meaning any control kind is technically valid here, not
  // just a nested if. This is acceptable since the grammar only ever produces
  // IfStmt as the else-if alternative, but tightening to unique_ptr<IfStmt>
  // would be safer.
  std::optional<
      std::variant<std::unique_ptr<CodeBlock>, std::unique_ptr<ControlStmt>>>
      else_branch;
};

struct WhileStmt : Node {
  Expression condition;
  std::unique_ptr<CodeBlock> block;
};

struct ForStmt : Node {
  Identifier var;
  Expression iterable;
  std::unique_ptr<CodeBlock> block;
};

struct Param : Node {
  Identifier name;
  std::string type;  // Could be a unit name or type identifier
};

struct FnDef : Node {
  Identifier name;
  std::vector<Param> params;
  std::optional<std::string> return_type;
  std::unique_ptr<CodeBlock> block;
};

struct StepStmt : Node {
  Identifier name;
  Param param;
  std::unique_ptr<CodeBlock> block;
};

struct ReturnStmt : Node {
  Expression value;
};

struct BreakStmt : Node {};
struct ContinueStmt : Node {};

// NOTE: IfStmt, WhileStmt, ForStmt, FnDef, StepStmt are held by value here.
// If any of these grow significantly, wrap them in unique_ptr<T> instead.
using ControlStmtData =
    std::variant<IfStmt, WhileStmt, ForStmt, FnDef, StepStmt, ReturnStmt,
                 BreakStmt, ContinueStmt>;

struct ControlStmt : Node {
  ControlStmtData data;
};

using StatementData = std::variant<CreateStmt, LetStmt, EnableStmt, ShowStmt,
                                   ControlStmt, AssignStmt>;

struct Statement : Node {
  StatementData data;
};

struct CodeBlock : Node {
  std::vector<Statement> statements;
};

struct Program {
  std::vector<Statement> statements;
};

// =========================================================================
// 3. Serialization Interface
// =========================================================================

/**
 * @brief Converts the AST program into a JSON-formatted string.
 */
std::string to_json(const Program& program);

}  // namespace whis::ast
