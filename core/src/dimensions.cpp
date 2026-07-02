#include "whis/dimensions.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>

#include "whis/ast.hpp"

namespace whis::dimensions {

// =========================================================================
// Internal helpers
// =========================================================================

// Formats a DimArray as a human-readable string, e.g. "[1,1,-2,0,0,0,0]".
static std::string fmt_dim(const ast::DimArray& d) {
  std::ostringstream os;
  os << '[';
  for (size_t i = 0; i < 7; ++i) {
    os << d[i];
    if (i != 6) os << ',';
  }
  os << ']';
  return os.str();
}

// Maps a DimArray to a physicist-friendly label for error messages.
static std::string dim_label(const ast::DimArray& d) {
  using namespace dims;
  if (d == kDimensionless) return "Dimensionless";
  if (d == kLength) return "Length";
  if (d == kMass) return "Mass";
  if (d == kTime) return "Time";
  if (d == kVelocity) return "Velocity";
  if (d == kAcceleration) return "Acceleration";
  if (d == kForce) return "Force";
  if (d == kEnergy) return "Energy";
  return fmt_dim(d);
}

// Element-wise addition of dimension exponent arrays (§2.2 multiplication).
static ast::DimArray dim_add(const ast::DimArray& a, const ast::DimArray& b) {
  ast::DimArray r{};
  for (size_t i = 0; i < 7; ++i) r[i] = static_cast<int16_t>(a[i] + b[i]);
  return r;
}

// Element-wise subtraction of dimension exponent arrays (§2.2 division).
static ast::DimArray dim_sub(const ast::DimArray& a, const ast::DimArray& b) {
  ast::DimArray r{};
  for (size_t i = 0; i < 7; ++i) r[i] = static_cast<int16_t>(a[i] - b[i]);
  return r;
}

// Scalar multiplication of exponent array (§2.2 power rule).
static ast::DimArray dim_scale(const ast::DimArray& a, int16_t n) {
  ast::DimArray r{};
  for (size_t i = 0; i < 7; ++i) r[i] = static_cast<int16_t>(a[i] * n);
  return r;
}

// =========================================================================
// Transcendental function set (§2.2 — must receive dimensionless operand)
// =========================================================================

static constexpr std::array<std::string_view, 7> kTranscendentals = {
    "sin", "cos", "tan", "ln", "log", "exp", "sqrt",
};

static bool is_transcendental(std::string_view name) {
  // Strip any dotted prefix: "math.sin" → "sin"
  const auto dot = name.rfind('.');
  const std::string_view bare =
      (dot == std::string_view::npos) ? name : name.substr(dot + 1);
  return std::any_of(kTranscendentals.begin(), kTranscendentals.end(),
                     [&](std::string_view t) { return t == bare; });
}

// =========================================================================
// Bottom-up expression dimension evaluator
// =========================================================================

// Forward-declared; visit_expr and visit_stmt are mutually recursive through
// CodeBlock.
struct DimChecker {
  // Computes and returns the DimArray for an expression node.
  // Throws DimError on any violation encountered during traversal.
  ast::DimArray visit_expr(const ast::Expression& e) {
    return std::visit(
        [this, &e](const auto& node) -> ast::DimArray {
          return dispatch(node, e.loc);
        },
        e.data);
  }

  // -----------------------------------------------------------------------
  // Expression node handlers
  // -----------------------------------------------------------------------

  // Literal physical values already carry their dimension from the suffix pass.
  ast::DimArray dispatch(const ast::DimensionedValue& n,
                         const ast::SourceLocation&) {
    return n.dimensions;
  }

  // Identifiers: dimension is unknown without a symbol table. Treat as
  // dimensionless so that identifier references do not produce false positives
  // before a symbol resolver is wired in.
  //
  // NOTE: Replace with symbol-table lookup once the scope resolver is
  // implemented (compiler pass §3 roadmap).
  ast::DimArray dispatch(const ast::Identifier&, const ast::SourceLocation&) {
    return dims::kDimensionless;
  }

  // Vectors: check that all components carry the same dimension; return that
  // shared dimension. A heterogeneous vector (e.g. (0 m, 1 s)) is treated
  // as a mismatch on the first differing component.
  ast::DimArray dispatch(const ast::VectorLiteral& n,
                         const ast::SourceLocation& loc) {
    if (n.elements.empty()) return dims::kDimensionless;

    const ast::DimArray base = visit_expr(n.elements[0]);
    for (size_t i = 1; i < n.elements.size(); ++i) {
      const ast::DimArray elem = visit_expr(n.elements[i]);
      // Dimensionless literals (bare 0, 1, …) are compatible with any
      // dimension inside a vector literal (e.g. normal: (0, 1)).
      if (elem == dims::kDimensionless) continue;
      if (base == dims::kDimensionless) continue;
      if (elem != base) {
        raise_mismatch("+", base, elem, loc);
      }
    }
    return base;
  }

  // Tweak expressions carry the same dimension as their initial value.
  ast::DimArray dispatch(const ast::TweakExpr& n, const ast::SourceLocation&) {
    return visit_expr(*n.initial);
  }

  // Call expressions: enforce dimensionless constraint for transcendental
  // functions; otherwise return dimensionless (return type unknown without
  // a type-indexed function registry).
  ast::DimArray dispatch(const ast::CallExpr& n,
                         const ast::SourceLocation& loc) {
    if (is_transcendental(n.name)) {
      for (const auto& arg : n.args) {
        const ast::DimArray d = visit_expr(arg);
        if (d != dims::kDimensionless) {
          std::ostringstream msg;
          msg << "E_UNIT_NON_TRANSCENDENT: Mathematical Boundary Error: "
              << "Arguments to '" << n.name << "()' must be fully dimensionless"
              << ". Received " << dim_label(d) << " " << fmt_dim(d) << " at "
              << loc.file << ':' << loc.line << ':' << loc.column << '.';
          throw DimError(ErrorCode::E_UNIT_NON_TRANSCENDENT, loc, msg.str());
        }
      }
    } else {
      // Visit args for side-effect error propagation even when not
      // transcendental; a mismatch inside an argument still fails.
      for (const auto& arg : n.args) visit_expr(arg);
    }
    // Return type inference requires a function registry; emit dimensionless
    // as a conservative approximation until that pass is implemented.
    return dims::kDimensionless;
  }

  // Binary expressions: apply §2.2 propagation rules per operator.
  ast::DimArray dispatch(const ast::BinaryExpr& n,
                         const ast::SourceLocation& loc) {
    const ast::DimArray lhs = visit_expr(*n.left);
    const ast::DimArray rhs = visit_expr(*n.right);

    switch (n.op) {
      // §2.2 Addition / Subtraction — operands must share the same dimension.
      case ast::BinaryOp::Add:
      case ast::BinaryOp::Sub: {
        // Allow dimensionless literals (bare numbers) to mix freely so that
        // patterns like `(x + 1)` in loop conditions do not false-positive.
        if (lhs == dims::kDimensionless || rhs == dims::kDimensionless)
          return (lhs == dims::kDimensionless) ? rhs : lhs;

        if (lhs != rhs) {
          const char* op_str = (n.op == ast::BinaryOp::Add) ? "+" : "-";
          raise_mismatch(op_str, lhs, rhs, loc);
        }
        return lhs;
      }

      // §2.2 Multiplication — element-wise addition of exponent arrays.
      case ast::BinaryOp::Mul:
        return dim_add(lhs, rhs);

      // §2.2 Division — element-wise subtraction of exponent arrays.
      case ast::BinaryOp::Div:
        return dim_sub(lhs, rhs);

      // §2.2 Power — scalar multiplication by the exponent value.
      // The exponent must be a compile-time numeric constant (DimensionedValue
      // with dimensionless array).
      case ast::BinaryOp::Pow: {
        const auto* exp_lit =
            std::get_if<ast::DimensionedValue>(&n.right->data);
        if (!exp_lit) {
          // Non-literal exponent: cannot propagate dimensions statically.
          // Emit dimensionless and let the runtime catch semantic errors.
          return dims::kDimensionless;
        }
        const auto exp_int = static_cast<int16_t>(exp_lit->value);
        return dim_scale(lhs, exp_int);
      }

      // Comparison operators: always produce a dimensionless boolean result.
      case ast::BinaryOp::Eq:
      case ast::BinaryOp::Ne:
      case ast::BinaryOp::Lt:
      case ast::BinaryOp::Le:
      case ast::BinaryOp::Gt:
      case ast::BinaryOp::Ge:
        // Still visit both sides for nested violation detection.
        return dims::kDimensionless;
    }

    return dims::kDimensionless;  // unreachable; satisfies compiler
  }

  // Unary negation: dimension passes through unchanged.
  ast::DimArray dispatch(const ast::UnaryExpr& n, const ast::SourceLocation&) {
    return visit_expr(*n.expr);
  }

  // -----------------------------------------------------------------------
  // Statement visitors (recurse into expression positions)
  // -----------------------------------------------------------------------

  void visit_stmt(const ast::Statement& s) {
    std::visit([this](const auto& node) { dispatch_stmt(node); }, s.data);
  }

  void visit_block(const ast::CodeBlock& b) {
    for (const auto& s : b.statements) visit_stmt(s);
  }

  void dispatch_stmt(const ast::LetStmt& n) {
    if (n.value) visit_expr(*n.value);
  }

  void dispatch_stmt(const ast::CreateStmt& n) {
    for (const auto& p : n.properties) visit_expr(p.value);
  }

  void dispatch_stmt(const ast::EnableStmt& n) {
    for (const auto& arg : n.args) {
      std::visit(
          [this](const auto& a) {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, ast::Property>) {
              visit_expr(a.value);
            } else {
              visit_expr(a);
            }
          },
          arg);
    }
  }

  void dispatch_stmt(const ast::ShowStmt& n) {
    for (const auto& a : n.args) visit_expr(a);
  }

  void dispatch_stmt(const ast::AssignStmt& n) { visit_expr(n.value); }

  void dispatch_stmt(const ast::ControlStmt& n) {
    std::visit([this](const auto& node) { dispatch_ctrl(node); }, n.data);
  }

  void dispatch_ctrl(const ast::IfStmt& n) {
    visit_expr(n.condition);
    if (n.then_block) visit_block(*n.then_block);
    if (n.else_branch) {
      std::visit(
          [this](const auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<ast::CodeBlock>>) {
              visit_block(*b);
            } else {
              dispatch_stmt(*b);
            }
          },
          *n.else_branch);
    }
  }

  void dispatch_ctrl(const ast::WhileStmt& n) {
    visit_expr(n.condition);
    if (n.block) visit_block(*n.block);
  }

  void dispatch_ctrl(const ast::ForStmt& n) {
    visit_expr(n.iterable);
    if (n.block) visit_block(*n.block);
  }

  void dispatch_ctrl(const ast::FnDef& n) {
    if (n.block) visit_block(*n.block);
  }

  void dispatch_ctrl(const ast::StepStmt& n) {
    if (n.block) visit_block(*n.block);
  }

  void dispatch_ctrl(const ast::ReturnStmt& n) { visit_expr(n.value); }

  // Terminal stmts carry no expressions.
  void dispatch_ctrl(const ast::BreakStmt&) {}
  void dispatch_ctrl(const ast::ContinueStmt&) {}

 private:
  [[noreturn]] void raise_mismatch(const char* op, const ast::DimArray& lhs,
                                   const ast::DimArray& rhs,
                                   const ast::SourceLocation& loc) {
    std::ostringstream msg;
    msg << "E_DIM_MISMATCH: Dimensional Analysis Violation: Cannot apply '"
        << op << "' to " << dim_label(lhs) << " " << fmt_dim(lhs) << " and "
        << dim_label(rhs) << " " << fmt_dim(rhs) << " at " << loc.file << ':'
        << loc.line << ':' << loc.column << '.';
    throw DimError(ErrorCode::E_DIM_MISMATCH, loc, msg.str());
  }
};

// =========================================================================
// Public entry point
// =========================================================================

void check(const ast::Program& program) {
  DimChecker checker;
  for (const auto& stmt : program.statements) {
    checker.visit_stmt(stmt);
  }
}

}  // namespace whis::dimensions