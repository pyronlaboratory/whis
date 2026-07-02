#include "whis/compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <tao/pegtl.hpp>
#include <unordered_map>

#include "whis/ast.hpp"
#include "whis/dimensions.hpp"
#include "whis/grammar.hpp"

namespace whis::compiler {

struct ParserState {
  // clang-format off
  whis::ast::Program                    program;
  std::vector<whis::ast::Expression>    expr_stack;
  std::vector<whis::ast::Property>      prop_stack;
  std::vector<whis::ast::ControlStmt>   control_stack;
  std::vector<std::string>              id_stack;
  std::vector<char>                     mul_div_ops;
  std::vector<char>                     add_sub_ops;
  std::vector<std::string>              cmp_ops;
  // clang-format on

  // Sentinel values used as stack frame markers. Each statement action pushes
  // a mark on entry via the grammar rule boundary; draining stops at the mark
  // so sibling statement residue is never consumed.
  static constexpr std::string_view kIdMark = "\x00__mark__";

  // Push a scope boundary onto both stacks before a statement rule fires.
  // Called from Action<K_let>, Action<K_create>, etc. at the top of apply().
  void push_mark() {
    id_stack.push_back(std::string(kIdMark));

    // Use a default-constructed Expression with a sentinel Identifier name.
    whis::ast::Expression sentinel;
    whis::ast::Identifier mark_id;
    mark_id.name = std::string(kIdMark);
    sentinel.data = std::move(mark_id);
    expr_stack.push_back(std::move(sentinel));
  }

  // Remove sentinel from id_stack and expr_stack.
  void pop_mark() {
    while (!id_stack.empty() && id_stack.back() != kIdMark) {
      id_stack.pop_back();
    }

    if (!id_stack.empty()) id_stack.pop_back();

    while (!expr_stack.empty() && !is_mark(expr_stack.back())) {
      expr_stack.pop_back();
    }

    if (!expr_stack.empty()) expr_stack.pop_back();
  }

  static bool is_mark(const whis::ast::Expression& e) {
    const auto* id = std::get_if<whis::ast::Identifier>(&e.data);
    return id && id->name == kIdMark;
  }

  // Drains expressions pushed after the most recent mark, oldest first.
  // Does not remove the mark itself.
  std::vector<whis::ast::Expression> drain_exprs_scoped() {
    std::vector<whis::ast::Expression> result;
    while (!expr_stack.empty() && !is_mark(expr_stack.back())) {
      result.insert(result.begin(), std::move(expr_stack.back()));
      expr_stack.pop_back();
    }
    return result;
  }

  // Drains ids pushed after the most recent mark, oldest first.
  std::vector<std::string> drain_ids_scoped() {
    std::vector<std::string> result;
    while (!id_stack.empty() && id_stack.back() != kIdMark) {
      result.insert(result.begin(), id_stack.back());
      id_stack.pop_back();
    }
    return result;
  }

  void push_expr(whis::ast::Expression&& e) {
    expr_stack.push_back(std::move(e));
  }

  whis::ast::Expression pop_expr() {
    // Never pop past the scope mark boundary.
    if (!expr_stack.empty() && is_mark(expr_stack.back())) {
      assert(false && "pop_expr reached scope mark: stack underflow");
    }
    assert(!expr_stack.empty() && "pop_expr called on empty stack");
    auto e = std::move(expr_stack.back());
    expr_stack.pop_back();
    return e;
  }
};

// SI scale factors: multiply parsed value by this to reach the SI baseline.
// e.g. 45 cm → 45 * 0.01 = 0.45 m
static const std::unordered_map<std::string, double> kUnitScale = {
    {"mm", 1e-3}, {"cm", 1e-2},  {"m", 1.0},    {"km", 1e3},
    {"mg", 1e-6}, {"g", 1e-3},   {"kg", 1.0},   {"ms", 1e-3},
    {"s", 1.0},   {"min", 60.0}, {"h", 3600.0}, {"A", 1.0},
    {"C", 1.0},   {"V", 1.0},    {"N", 1.0},    {"J", 1.0},
    {"W", 1.0},   {"Pa", 1.0},   {"rad", 1.0},  {"deg", M_PI / 180.0},
};

// Dimension exponent arrays [M, L, T, I, Θ, N, J] for each base unit atom.
// Compound suffixes like m/s^2 are resolved by apply_suffix() below.
static const std::unordered_map<std::string, whis::ast::DimArray> kUnitDims = {
    {"mm", {0, 1, 0, 0, 0, 0, 0}},   {"cm", {0, 1, 0, 0, 0, 0, 0}},
    {"m", {0, 1, 0, 0, 0, 0, 0}},    {"km", {0, 1, 0, 0, 0, 0, 0}},
    {"mg", {1, 0, 0, 0, 0, 0, 0}},   {"g", {1, 0, 0, 0, 0, 0, 0}},
    {"kg", {1, 0, 0, 0, 0, 0, 0}},   {"ms", {0, 0, 1, 0, 0, 0, 0}},
    {"s", {0, 0, 1, 0, 0, 0, 0}},    {"min", {0, 0, 1, 0, 0, 0, 0}},
    {"h", {0, 0, 1, 0, 0, 0, 0}},    {"A", {0, 0, 0, 1, 0, 0, 0}},
    {"C", {0, 0, 1, 1, 0, 0, 0}},    {"N", {1, 1, -2, 0, 0, 0, 0}},
    {"J", {1, 2, -2, 0, 0, 0, 0}},   {"W", {1, 2, -3, 0, 0, 0, 0}},
    {"Pa", {1, -1, -2, 0, 0, 0, 0}}, {"V", {1, 2, -3, -1, 0, 0, 0}},
    {"rad", {0, 0, 0, 0, 0, 0, 0}},  {"deg", {0, 0, 0, 0, 0, 0, 0}},
};

// Parses a suffix string like "m/s^2" or "N*m^2/kg^2" and applies SI scale
// and dimension arithmetic (§2.2) to the DimensionedValue on top of the stack.
//
// Suffix grammar:  SuffixMul [ '/' SuffixMul ]
// SuffixMul:       SuffixTerm [ '*' SuffixTerm ]*
// SuffixTerm:      BaseUnit [ '^' ['-'] NumericLiteral ]
//
// We parse the matched string manually here rather than adding per-sub-rule
// actions, because SuffixTerm and SuffixMul fire for every sub-expression
// inside a compound suffix and would require their own stack management.
static void apply_suffix(const std::string& suffix_str,
                         whis::ast::DimensionedValue& dv) {
  // Split on '/' to separate numerator and denominator SuffixMul groups.
  const size_t slash = suffix_str.find('/');
  const std::string num_str = suffix_str.substr(0, slash);
  const std::string den_str =
      (slash != std::string::npos) ? suffix_str.substr(slash + 1) : "";

  // Parses one SuffixMul string (e.g. "N*m^2") into a cumulative scale
  // factor and dimension array, applying exponents.
  auto parse_mul = [&](const std::string& s,
                       int sign) -> std::pair<double, whis::ast::DimArray> {
    double scale = 1.0;
    whis::ast::DimArray dims = {};

    // Tokenise on '*'.
    size_t pos = 0;
    while (pos < s.size()) {
      const size_t star = s.find('*', pos);
      const std::string term = s.substr(
          pos, star == std::string::npos ? std::string::npos : star - pos);
      pos = (star == std::string::npos) ? s.size() : star + 1;

      // Split term on '^'.
      const size_t caret = term.find('^');
      const std::string unit = term.substr(0, caret);

      // Parse exponent, defaulting to 1 if absent.
      int exp = 1;
      if (caret != std::string::npos) {
        exp = std::stoi(term.substr(caret + 1));
      }

      const int net_exp = sign * exp;

      // Apply scale: base_scale ^ exponent.
      auto sit = kUnitScale.find(unit);
      if (sit != kUnitScale.end()) {
        double base = sit->second;
        for (int i = 0; i < std::abs(exp); ++i)
          scale *= (exp > 0) ? base : (1.0 / base);
      }

      // Apply dimension exponents (§2.2 multiplication rule).
      auto dit = kUnitDims.find(unit);
      if (dit != kUnitDims.end()) {
        for (size_t i = 0; i < 7; ++i) {
          dims[i] = static_cast<int16_t>(dims[i] + net_exp * dit->second[i]);
        }
      }
    }
    return {scale, dims};
  };

  auto [num_scale, num_dims] = parse_mul(num_str, +1);
  double total_scale = num_scale;
  whis::ast::DimArray total_dims = num_dims;

  if (!den_str.empty()) {
    auto [den_scale, den_dims] = parse_mul(den_str, -1);
    total_scale /= den_scale;
    for (size_t i = 0; i < 7; ++i) {
      // Division subtracts exponents (§2.2), already negated by sign=-1 above.
      total_dims[i] = static_cast<int16_t>(total_dims[i] + den_dims[i]);
    }
  }

  dv.value *= total_scale;
  dv.dimensions = total_dims;
}

template <typename Rule>
struct Action : tao::pegtl::nothing<Rule> {};

template <typename Input>
whis::ast::SourceLocation get_loc(const Input& in) {
  return {in.position().source, (uint32_t)in.position().line,
          (uint32_t)in.position().column};
}

template <>
struct Action<whis::grammar::OpMulDiv> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    state.mul_div_ops.push_back(in.string()[0]);
  }
};

template <>
struct Action<whis::grammar::OpAddSub> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    state.add_sub_ops.push_back(in.string()[0]);
  }
};

template <>
struct Action<whis::grammar::OpCmp> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    state.cmp_ops.push_back(in.string());
  }
};

template <>
struct Action<whis::grammar::DottedName> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    std::string full_name = in.string();

    whis::ast::Identifier id;
    id.loc = get_loc(in);
    id.name = full_name;
    state.id_stack.push_back(id.name);

    whis::ast::Expression e;
    e.loc = id.loc;
    e.data = std::move(id);
    state.push_expr(std::move(e));
  }
};

template <>
struct Action<whis::grammar::TweakExpr> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    if (state.expr_stack.size() >= 3) {
      auto max = std::make_unique<whis::ast::Expression>(state.pop_expr());
      auto min = std::make_unique<whis::ast::Expression>(state.pop_expr());
      auto initial = std::make_unique<whis::ast::Expression>(state.pop_expr());

      if (!state.id_stack.empty() && state.id_stack.back() == "tweak") {
        state.id_stack.pop_back();
        if (!state.expr_stack.empty()) {
          const auto& back = state.expr_stack.back();
          if (const auto* id = std::get_if<whis::ast::Identifier>(&back.data)) {
            if (id->name == "tweak") {
              state.expr_stack.pop_back();
            }
          }
        }
      }

      whis::ast::TweakExpr te;
      te.loc = get_loc(in);
      te.initial = std::move(initial);
      te.min = std::move(min);
      te.max = std::move(max);

      whis::ast::Expression e;
      e.loc = te.loc;
      e.data = std::move(te);
      state.push_expr(std::move(e));
    }
  }
};

template <>
struct Action<whis::grammar::DottedCall> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    std::string str = in.string();
    size_t first_paren = str.find('(');
    size_t last_paren = str.rfind(')');
    std::string args_str =
        str.substr(first_paren + 1, last_paren - first_paren - 1);

    int depth = 0;
    int commas = 0;
    bool empty = true;
    size_t i = 0;
    while (i < args_str.size()) {
      if (std::isspace(args_str[i])) {
        ++i;
      } else if (i + 1 < args_str.size() && args_str[i] == '/' &&
                 args_str[i + 1] == '/') {
        i = args_str.find_first_of("\r\n", i);
        if (i == std::string::npos) break;
      } else {
        empty = false;
        if (args_str[i] == '(' || args_str[i] == '[')
          ++depth;
        else if (args_str[i] == ')' || args_str[i] == ']')
          --depth;
        else if (args_str[i] == ',' && depth == 0)
          ++commas;
        ++i;
      }
    }
    int arg_count = empty ? 0 : (commas + 1);

    std::vector<whis::ast::Expression> args;
    for (int j = 0; j < arg_count && !state.expr_stack.empty(); ++j) {
      args.push_back(state.pop_expr());
    }
    std::reverse(args.begin(), args.end());

    std::string fn_name;
    if (!state.expr_stack.empty()) {
      auto name_expr = state.pop_expr();
      if (auto* id = std::get_if<whis::ast::Identifier>(&name_expr.data)) {
        fn_name = id->name;
      }
    }
    if (!state.id_stack.empty() && state.id_stack.back() == fn_name) {
      state.id_stack.pop_back();
    }

    whis::ast::CallExpr ce;
    ce.loc = get_loc(in);
    ce.name = fn_name;
    ce.args = std::move(args);

    whis::ast::Expression e;
    e.loc = ce.loc;
    e.data = std::move(ce);
    state.push_expr(std::move(e));
  }
};

template <>
struct Action<whis::grammar::Factor> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    std::string src = in.string();
    size_t start = src.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return;
    size_t end = src.find_last_not_of(" \t\r\n");
    src = src.substr(start, end - start + 1);

    bool has_power = false;
    size_t pos = 0;
    while ((pos = src.find('^', pos)) != std::string::npos) {
      bool preceded_by_unit = false;
      for (const auto& [unit, _] : kUnitScale) {
        if (pos >= unit.size() &&
            src.substr(pos - unit.size(), unit.size()) == unit) {
          size_t unit_start = pos - unit.size();
          if (unit_start == 0 || (!std::isalnum(src[unit_start - 1]) &&
                                  src[unit_start - 1] != '_')) {
            preceded_by_unit = true;
            break;
          }
        }
      }
      if (!preceded_by_unit) {
        has_power = true;
        break;
      }
      pos++;
    }

    if (has_power) {
      if (state.expr_stack.size() >= 2) {
        auto right = std::make_unique<whis::ast::Expression>(state.pop_expr());
        auto left = std::make_unique<whis::ast::Expression>(state.pop_expr());
        whis::ast::BinaryExpr be;
        be.loc = get_loc(in);
        be.op = whis::ast::BinaryOp::Pow;
        be.left = std::move(left);
        be.right = std::move(right);

        whis::ast::Expression e;
        e.loc = be.loc;
        e.data = std::move(be);
        state.push_expr(std::move(e));
      }
    }

    if (src[0] == '-') {
      if (!state.expr_stack.empty()) {
        auto inner = std::make_unique<whis::ast::Expression>(state.pop_expr());
        whis::ast::UnaryExpr ue;
        ue.loc = get_loc(in);
        ue.op = '-';
        ue.expr = std::move(inner);

        whis::ast::Expression e;
        e.loc = ue.loc;
        e.data = std::move(ue);
        state.push_expr(std::move(e));
      }
    }
  }
};

template <>
struct Action<whis::grammar::Term> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    if (state.mul_div_ops.empty()) return;

    size_t num_ops = state.mul_div_ops.size();
    std::vector<char> ops = std::move(state.mul_div_ops);
    state.mul_div_ops.clear();

    std::vector<whis::ast::Expression> exprs;
    for (size_t i = 0; i <= num_ops && !state.expr_stack.empty(); ++i) {
      exprs.push_back(state.pop_expr());
    }
    std::reverse(exprs.begin(), exprs.end());

    whis::ast::Expression current = std::move(exprs[0]);
    for (size_t i = 0; i < num_ops; ++i) {
      char op_char = ops[i];
      whis::ast::BinaryExpr be;
      be.loc = current.loc;
      be.op = (op_char == '*') ? whis::ast::BinaryOp::Mul
                               : whis::ast::BinaryOp::Div;
      be.left = std::make_unique<whis::ast::Expression>(std::move(current));
      be.right =
          std::make_unique<whis::ast::Expression>(std::move(exprs[i + 1]));

      current.loc = be.loc;
      current.data = std::move(be);
    }
    state.push_expr(std::move(current));
  }
};

template <>
struct Action<whis::grammar::ArithExpr> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    if (state.add_sub_ops.empty()) return;

    size_t num_ops = state.add_sub_ops.size();
    std::vector<char> ops = std::move(state.add_sub_ops);
    state.add_sub_ops.clear();

    std::vector<whis::ast::Expression> exprs;
    for (size_t i = 0; i <= num_ops && !state.expr_stack.empty(); ++i) {
      exprs.push_back(state.pop_expr());
    }
    std::reverse(exprs.begin(), exprs.end());

    whis::ast::Expression current = std::move(exprs[0]);
    for (size_t i = 0; i < num_ops; ++i) {
      char op_char = ops[i];
      whis::ast::BinaryExpr be;
      be.loc = current.loc;
      be.op = (op_char == '+') ? whis::ast::BinaryOp::Add
                               : whis::ast::BinaryOp::Sub;
      be.left = std::make_unique<whis::ast::Expression>(std::move(current));
      be.right =
          std::make_unique<whis::ast::Expression>(std::move(exprs[i + 1]));

      current.loc = be.loc;
      current.data = std::move(be);
    }
    state.push_expr(std::move(current));
  }
};

template <>
struct Action<whis::grammar::Expression> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    if (state.cmp_ops.empty()) return;

    std::string op_str = state.cmp_ops.back();
    state.cmp_ops.pop_back();

    if (state.expr_stack.size() >= 2) {
      auto right = std::make_unique<whis::ast::Expression>(state.pop_expr());
      auto left = std::make_unique<whis::ast::Expression>(state.pop_expr());

      whis::ast::BinaryExpr be;
      be.loc = get_loc(in);
      if (op_str == "==")
        be.op = whis::ast::BinaryOp::Eq;
      else if (op_str == "!=")
        be.op = whis::ast::BinaryOp::Ne;
      else if (op_str == "<")
        be.op = whis::ast::BinaryOp::Lt;
      else if (op_str == "<=")
        be.op = whis::ast::BinaryOp::Le;
      else if (op_str == ">")
        be.op = whis::ast::BinaryOp::Gt;
      else if (op_str == ">=")
        be.op = whis::ast::BinaryOp::Ge;

      be.left = std::move(left);
      be.right = std::move(right);

      whis::ast::Expression e;
      e.loc = be.loc;
      e.data = std::move(be);
      state.push_expr(std::move(e));
    }
  }
};

// Suffix fires after Factor has already pushed the numeric DimensionedValue.
// We find that value on top of expr_stack and apply the SI transformation.
template <>
struct Action<whis::grammar::Suffix> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    if (state.expr_stack.empty()) return;

    // Walk from bottom up (oldest first) to find the numeric value this
    // suffix belongs to, not the exponent literals pushed by SuffixTerm.
    for (size_t i = 0; i < state.expr_stack.size(); ++i) {
      auto* dv =
          std::get_if<whis::ast::DimensionedValue>(&state.expr_stack[i].data);

      if (dv && !dv->suffix_applied) {
        apply_suffix(in.string(), *dv);
        dv->suffix_applied = true;
        return;
      }
    }
  }
};

template <>
struct Action<whis::grammar::Identifier> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::Identifier id;
    id.loc = get_loc(in);
    id.name = in.string();
    state.id_stack.push_back(id.name);

    whis::ast::Expression e;
    e.loc = id.loc;
    e.data = std::move(id);
    state.push_expr(std::move(e));
  }
};

template <>
struct Action<whis::grammar::NumericLiteral> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::DimensionedValue val;
    val.loc = get_loc(in);
    val.value = std::stod(in.string());
    val.dimensions.fill(0);

    whis::ast::Expression e;
    e.loc = val.loc;
    e.data = std::move(val);
    state.push_expr(std::move(e));
  }
};

// VectorLiteral grammar matches: '(' Expr ',' Expr [',' Expr] ')'
// By the time this action fires, the 2 or 3 component expressions have been
// pushed onto expr_stack by their own sub-rule actions. We pop exactly that
// many, stopping when we hit a non-DimensionedValue/non-numeric boundary
// isn't reliable, so we track component count via the matched source string.
//
// Reliable count: count commas at depth 0 inside the matched string.
template <>
struct Action<whis::grammar::VectorLiteral> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    // Count top-level commas to determine 2D vs 3D.
    int depth = 0;
    int commas = 0;
    for (char c : in.string()) {
      if (c == '(' || c == '[')
        ++depth;
      else if (c == ')' || c == ']')
        --depth;
      else if (c == ',' && depth == 1)
        ++commas;
    }
    const int component_count = commas + 1;  // 2 or 3

    whis::ast::VectorLiteral vl;
    vl.loc = get_loc(in);

    // Pop components in reverse order, then reverse to get source order.
    std::vector<whis::ast::Expression> components;
    for (int i = 0; i < component_count && !state.expr_stack.empty(); ++i) {
      components.push_back(state.pop_expr());
    }
    std::reverse(components.begin(), components.end());

    vl.elements = std::move(components);

    whis::ast::Expression e;
    e.loc = vl.loc;
    e.data = std::move(vl);
    state.push_expr(std::move(e));
  }
};

#define WHIS_MARK_ACTION(Rule)                               \
  template <>                                                \
  struct Action<whis::grammar::Rule> {                       \
    template <typename Input>                                \
    static void apply(const Input& in, ParserState& state) { \
      (void)in;                                              \
      state.push_mark();                                     \
    }                                                        \
  }

WHIS_MARK_ACTION(K_create);
WHIS_MARK_ACTION(K_let);
WHIS_MARK_ACTION(K_enable);
WHIS_MARK_ACTION(K_disable);
WHIS_MARK_ACTION(K_show);
WHIS_MARK_ACTION(K_hide);

#undef WHIS_MARK_ACTION

template <>
struct Action<whis::grammar::CreateStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::CreateStmt stmt;
    stmt.loc = get_loc(in);

    // Drain properties first, they were pushed by PropertyPair actions.
    while (!state.prop_stack.empty()) {
      stmt.properties.insert(stmt.properties.begin(),
                             std::move(state.prop_stack.back()));
      state.prop_stack.pop_back();
    }

    // The entity name is ids[0], the first identifier pushed after the mark,
    // which is the name token before the '{'. Property key identifiers are
    // consumed inside PropertyPair action and removed from id_stack there,
    // so ids[0] reliably holds only the entity name.
    auto ids = state.drain_ids_scoped();
    if (!ids.empty()) stmt.type.name = ids[0];

    state.pop_mark();

    whis::ast::Statement s;
    s.loc = stmt.loc;
    s.data = std::move(stmt);
    state.program.statements.push_back(std::move(s));
  }
};

template <>
struct Action<whis::grammar::LetStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::LetStmt stmt;
    stmt.loc = get_loc(in);

    auto exprs = state.drain_exprs_scoped();  // everything after the mark
    auto ids = state.drain_ids_scoped();

    // exprs[0] is always the name identifier expression (pushed by Identifier
    // action). exprs[1], if present, is the init value expression. ids[0] is
    // the name string.
    if (ids.size() >= 1) stmt.name.name = ids[0];
    if (exprs.size() >= 2) stmt.value = std::move(exprs[1]);

    state.pop_mark();

    whis::ast::Statement s;
    s.loc = stmt.loc;
    s.data = std::move(stmt);
    state.program.statements.push_back(std::move(s));
  }
};

template <>
struct Action<whis::grammar::EnableStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::EnableStmt stmt;
    stmt.loc = get_loc(in);
    stmt.is_enable = (in.string().substr(0, 6) == "enable");

    // drain_exprs_scoped stops at the mark, no bleed from prior statements.
    auto exprs = state.drain_exprs_scoped();
    auto ids = state.drain_ids_scoped();

    // ids[0] is the target name; exprs[0] is its identifier expression, skip
    // it. Remaining exprs are the positional arguments.
    if (!ids.empty()) stmt.name.name = ids[0];
    for (size_t i = 1; i < exprs.size(); ++i) {
      stmt.args.push_back(std::move(exprs[i]));
    }

    state.pop_mark();

    whis::ast::Statement s;
    s.loc = stmt.loc;
    s.data = std::move(stmt);
    state.program.statements.push_back(std::move(s));
  }
};

template <>
struct Action<whis::grammar::ShowStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::ShowStmt stmt;
    stmt.loc = get_loc(in);
    stmt.is_show = (in.string().substr(0, 4) == "show");

    auto exprs = state.drain_exprs_scoped();
    auto ids = state.drain_ids_scoped();

    // ids[0] = target name; remaining exprs (skip index 0, the name expr)
    // are the expression arguments.
    if (!ids.empty()) stmt.name.name = ids[0];
    for (size_t i = 1; i < exprs.size(); ++i) {
      stmt.args.push_back(std::move(exprs[i]));
    }

    state.pop_mark();

    whis::ast::Statement s;
    s.loc = stmt.loc;
    s.data = std::move(stmt);
    state.program.statements.push_back(std::move(s));
  }
};

// Push a nested mark before each PropertyPair so each pair drains only its
// own tokens without consuming siblings.
template <>
struct Action<whis::grammar::PropertyPair> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::Property prop;
    prop.loc = get_loc(in);

    // PropertyPair fires inside CreateStmt's scope mark.
    // The pair matched is: Identifier ':' Expression
    // The Identifier action pushed both to id_stack and expr_stack.
    // The expression value is on top; the name identifier expression is below.
    // We take the top expr as value, and the top id as key, then discard the
    // key's residual expr entry so it doesn't pollute the enclosing scope.

    auto exprs = state.drain_exprs_scoped();  // [name_expr, value_expr]
    auto ids = state.drain_ids_scoped();      // [key_name]

    if (!ids.empty()) prop.name.name = ids[0];
    if (exprs.size() >= 2) prop.value = std::move(exprs[1]);

    // PropertyPair does not pop_mark, it shares the CreateStmt scope.
    state.prop_stack.push_back(std::move(prop));
  }
};

template <>
struct Action<whis::grammar::AssignStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    whis::ast::AssignStmt stmt;
    stmt.loc = get_loc(in);

    if (!state.expr_stack.empty()) {
      stmt.value = state.pop_expr();
    }
    // Target is a dotted name, use id_stack top.
    if (!state.id_stack.empty()) {
      stmt.target = state.id_stack.back();
      state.id_stack.pop_back();
    }

    whis::ast::Statement s;
    s.loc = stmt.loc;
    s.data = std::move(stmt);
    state.program.statements.push_back(std::move(s));
  }
};

// K_break and K_continue must NOT have action specialisations.
//
// The keyword match fires mid-rule inside ControlStmt's sor<>, if we push
// to control_stack here, the node is never drained into the program because
// there is no CodeBlock action to claim it.
//
// Instead, ControlStmt inspects its own matched string to construct the
// terminal variants.
template <>
struct Action<whis::grammar::ControlStmt> {
  template <typename Input>
  static void apply(const Input& in, ParserState& state) {
    const std::string src = in.string();
    whis::ast::ControlStmt cs;
    cs.loc = get_loc(in);

    if (src.substr(0, 5) == "break") {
      whis::ast::BreakStmt brk;
      brk.loc = cs.loc;
      cs.data = std::move(brk);
    } else if (src.substr(0, 8) == "continue") {
      whis::ast::ContinueStmt cont;
      cont.loc = cs.loc;
      cs.data = std::move(cont);
    } else {
      // Structural control nodes (while/if/for/fn/step/return) are assembled
      // by their dedicated sub-actions in a later pass. For now push a
      // placeholder so the statement count is correct.

      // TODO: assemble full structural control nodes here once sub-actions
      // for CodeBlock are implemented.
      whis::ast::BreakStmt placeholder;
      placeholder.loc = cs.loc;
      cs.data = std::move(placeholder);
    }

    whis::ast::Statement s;
    s.loc = cs.loc;
    s.data = std::move(cs);
    state.program.statements.push_back(std::move(s));
  }
};

ParseResult parse_only(const std::string& source) {
  if (source.empty()) return {false, "Empty source input buffer", 1, 1, {}};

  tao::pegtl::string_input<> input(source, "whis_spec_target");
  ParserState state;
  try {
    bool ok = tao::pegtl::parse<whis::grammar::Program, Action>(input, state);
    if (!ok)
      return {false,
              "Grammar structure mismatched top-level rules (returned false)",
              input.position().line, input.position().column,
              std::move(state.program)};
    return {true, "", 0, 0, std::move(state.program)};
  } catch (const tao::pegtl::parse_error& e) {
    size_t l = 1, c = 1;
    if (!e.positions().empty()) {
      l = e.positions().front().line;
      c = e.positions().front().column;
    }
    return {false, e.what(), l, c, std::move(state.program)};
  }
}

ParseResult generate_ast(const std::string& source) {
  if (source.empty()) {
    return {false, "Empty source input buffer", 1, 1, {}};
  }

  tao::pegtl::string_input<> input(source, "whis_spec_target");
  ParserState state;

  try {
    bool ok = tao::pegtl::parse<whis::grammar::Program, Action>(input, state);

    if (!ok) {
      return {false,
              "Grammar structure mismatched top-level rules (returned false)",
              input.position().line, input.position().column,
              std::move(state.program)};
    }

    // §2.1 / §2.2 — run compile-time dimensional analysis pass over the
    // fully-constructed AST before handing it to downstream consumers.
    // Violations surface as DimError; map them into the standard ParseResult
    // diagnostic fields so callers observe a uniform failure contract.
    whis::dimensions::check(state.program);

    return {true, "", 0, 0, std::move(state.program)};
  } catch (const whis::dimensions::DimError& e) {
    return {false, e.what(), e.loc.line, e.loc.column,
            std::move(state.program)};
  } catch (const tao::pegtl::parse_error& e) {
    size_t error_line = 1;
    size_t error_col = 1;
    if (!e.positions().empty()) {
      const auto pos = e.positions().front();
      error_line = pos.line;
      error_col = pos.column;
    }
    return {false, e.what(), error_line, error_col, std::move(state.program)};
  }
}

struct UnparseVisitor {
  std::string result;

  void operator()(const whis::ast::Identifier& n) { result += n.name; }
  void operator()(const whis::ast::DimensionedValue& n) {
    result += std::to_string(n.value);
  }
  void operator()(const whis::ast::VectorLiteral& n) {
    result += "(";
    for (size_t i = 0; i < n.elements.size(); ++i) {
      std::visit(*this, n.elements[i].data);
      if (i != n.elements.size() - 1) result += ", ";
    }
    result += ")";
  }
  void operator()(const whis::ast::TweakExpr&) {
    result += "tweak(0, range: 0..1)";
  }
  void operator()(const whis::ast::CallExpr& n) { result += n.name + "()"; }
  void operator()(const whis::ast::BinaryExpr& n) {
    // TODO: use n.op to emit the correct operator symbol.
    // Currently a stub, all binary expressions unparse as '+'.
    std::visit(*this, n.left->data);
    result += " + ";
    std::visit(*this, n.right->data);
  }
  void operator()(const whis::ast::UnaryExpr& n) {
    result += n.op;
    std::visit(*this, n.expr->data);
  }

  void operator()(const whis::ast::CreateStmt& n) {
    result += "create " + n.type.name + " { }";
  }
  void operator()(const whis::ast::LetStmt& n) {
    result += "let " + n.name.name;
    if (n.value) {
      result += " = ";
      std::visit(*this, n.value->data);
    }
    result += ";\n";
  }
  void operator()(const whis::ast::EnableStmt& n) {
    result += (n.is_enable ? "enable " : "disable ") + n.name.name + "(";
    for (size_t i = 0; i < n.args.size(); ++i) {
      std::visit(
          [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, whis::ast::Property>) {
              result += arg.name.name + ": ";
              std::visit(*this, arg.value.data);
            } else {
              std::visit(*this, arg.data);
            }
          },
          n.args[i]);
      if (i != n.args.size() - 1) result += ", ";
    }
    result += ");\n";
  }
  void operator()(const whis::ast::ShowStmt&) { result += "show debug;\n"; }
  void operator()(const whis::ast::ControlStmt&) {
    result += "if (true) { }\n";
  }
  void operator()(const whis::ast::AssignStmt& n) {
    result += n.target + " = 0;\n";
  }
};

std::string generate_source(const whis::ast::Program& ast) {
  UnparseVisitor visitor;
  for (const auto& stmt : ast.statements) {
    std::visit(visitor, stmt.data);
  }
  return visitor.result;
}

}  // namespace whis::compiler
