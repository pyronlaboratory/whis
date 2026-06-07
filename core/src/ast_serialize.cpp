#include <iomanip>
#include <sstream>

#include "whis/ast.hpp"

namespace whis::ast {

static std::string json_escape(const std::string& s) {
  std::ostringstream o;
  for (auto c : s) {
    switch (c) {
      case '"':
        o << "\\\"";
        break;
      case '\\':
        o << "\\\\";
        break;
      case '\b':
        o << "\\b";
        break;
      case '\f':
        o << "\\f";
        break;
      case '\n':
        o << "\\n";
        break;
      case '\r':
        o << "\\r";
        break;
      case '\t':
        o << "\\t";
        break;
      default:
        if ('\x00' <= c && c <= '\x1f') {
          o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        } else {
          o << c;
        }
    }
  }
  return o.str();
}

static void write_loc(std::ostream& os, const SourceLocation& loc) {
  os << "\"loc\":{\"file\":\"" << json_escape(loc.file)
     << "\",\"line\":" << loc.line << ",\"column\":" << loc.column << "}";
}

struct ToJsonVisitor {
  std::ostream& os;

  void operator()(const Identifier& n) {
    os << "{\"node\":\"Identifier\",\"name\":\"" << json_escape(n.name)
       << "\",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const DimensionedValue& n) {
    os << "{\"node\":\"DimensionedValue\",\"value\":" << n.value
       << ",\"dimensions\":[";
    for (size_t i = 0; i < 7; ++i) {
      os << n.dimensions[i] << (i == 6 ? "" : ",");
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const VectorLiteral& n) {
    os << "{\"node\":\"VectorLiteral\",\"elements\":[";
    for (size_t i = 0; i < n.elements.size(); ++i) {
      visit_expr(n.elements[i]);
      if (i != n.elements.size() - 1) os << ",";
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const TweakExpr& n) {
    os << "{\"node\":\"TweakExpr\",\"initial\":";
    visit_expr(*n.initial);
    os << ",\"min\":";
    visit_expr(*n.min);
    os << ",\"max\":";
    visit_expr(*n.max);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const CallExpr& n) {
    os << "{\"node\":\"CallExpr\",\"name\":\"" << json_escape(n.name)
       << "\",\"args\":[";
    for (size_t i = 0; i < n.args.size(); ++i) {
      visit_expr(n.args[i]);
      if (i != n.args.size() - 1) os << ",";
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const BinaryExpr& n) {
    os << "{\"node\":\"BinaryExpr\",\"op\":\"";
    switch (n.op) {
      case BinaryOp::Add:
        os << "+";
        break;
      case BinaryOp::Sub:
        os << "-";
        break;
      case BinaryOp::Mul:
        os << "*";
        break;
      case BinaryOp::Div:
        os << "/";
        break;
      case BinaryOp::Pow:
        os << "^";
        break;
      case BinaryOp::Eq:
        os << "==";
        break;
      case BinaryOp::Ne:
        os << "!=";
        break;
      case BinaryOp::Lt:
        os << "<";
        break;
      case BinaryOp::Le:
        os << "<=";
        break;
      case BinaryOp::Gt:
        os << ">";
        break;
      case BinaryOp::Ge:
        os << ">=";
        break;
    }
    os << "\",\"left\":";
    visit_expr(*n.left);
    os << ",\"right\":";
    visit_expr(*n.right);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const UnaryExpr& n) {
    os << "{\"node\":\"UnaryExpr\",\"op\":\"" << n.op << "\",\"expr\":";
    visit_expr(*n.expr);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void visit_expr(const Expression& e) { std::visit(*this, e.data); }

  void visit_stmt(const Statement& s) { std::visit(*this, s.data); }

  void operator()(const CreateStmt& n) {
    os << "{\"node\":\"CreateStmt\",\"type\":";
    (*this)(n.type);
    os << ",\"properties\":[";
    for (size_t i = 0; i < n.properties.size(); ++i) {
      os << "{\"name\":";
      (*this)(n.properties[i].name);
      os << ",\"value\":";
      visit_expr(n.properties[i].value);
      os << "}";
      if (i != n.properties.size() - 1) os << ",";
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const LetStmt& n) {
    os << "{\"node\":\"LetStmt\",\"name\":";
    (*this)(n.name);
    os << ",\"value\":";
    if (n.value) {
      visit_expr(*n.value);
    } else {
      os << "null";
    }
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const EnableStmt& n) {
    os << "{\"node\":\"EnableStmt\",\"is_enable\":"
       << (n.is_enable ? "true" : "false") << ",\"name\":";
    (*this)(n.name);
    os << ",\"args\":[";
    for (size_t i = 0; i < n.args.size(); ++i) {
      std::visit(
          [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Property>) {
              os << "{\"property\":{\"name\":";
              (*this)(arg.name);
              os << ",\"value\":";
              visit_expr(arg.value);
              os << "}}";
            } else {
              visit_expr(arg);
            }
          },
          n.args[i]);
      if (i != n.args.size() - 1) os << ",";
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const ShowStmt& n) {
    os << "{\"node\":\"ShowStmt\",\"is_show\":"
       << (n.is_show ? "true" : "false") << ",\"name\":";
    (*this)(n.name);
    os << ",\"args\":[";
    for (size_t i = 0; i < n.args.size(); ++i) {
      visit_expr(n.args[i]);
      if (i != n.args.size() - 1) os << ",";
    }
    os << "],\"on_target\":";
    if (n.on_target) {
      (*this)(*n.on_target);
    } else {
      os << "null";
    }
    os << ",\"resolution\":";
    if (n.resolution) {
      os << "{\"name\":";
      (*this)(n.resolution->name);
      os << ",\"dims\":[";
      for (size_t i = 0; i < n.resolution->dims.size(); ++i) {
        (*this)(n.resolution->dims[i]);
        if (i != n.resolution->dims.size() - 1) os << ",";
      }
      os << "]}";
    } else {
      os << "null";
    }
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const AssignStmt& n) {
    os << "{\"node\":\"AssignStmt\",\"target\":\"" << json_escape(n.target)
       << "\",\"value\":";
    visit_expr(n.value);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  // Dispatches into ControlStmt::data for the actual control node type.
  // Chain: visit_stmt → operator()(ControlStmt) → std::visit →
  // operator()(IfStmt / WhileStmt / etc.)
  void operator()(const ControlStmt& n) { std::visit(*this, n.data); }

  void operator()(const IfStmt& n) {
    os << "{\"node\":\"IfStmt\",\"condition\":";
    visit_expr(n.condition);
    os << ",\"then_block\":";
    visit_block(*n.then_block);
    os << ",\"else_branch\":";
    if (n.else_branch) {
      std::visit(
          [this](auto&& branch) {
            using T = std::decay_t<decltype(branch)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<CodeBlock>>) {
              visit_block(*branch);
            } else {
              // branch is unique_ptr<ControlStmt>; the grammar only places an
              // IfStmt here in practice, but any ControlStmt kind is accepted.
              (*this)(*branch);
            }
          },
          *n.else_branch);
    } else {
      os << "null";
    }
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const WhileStmt& n) {
    os << "{\"node\":\"WhileStmt\",\"condition\":";
    visit_expr(n.condition);
    os << ",\"block\":";
    visit_block(*n.block);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const ForStmt& n) {
    os << "{\"node\":\"ForStmt\",\"var\":";
    (*this)(n.var);
    os << ",\"iterable\":";
    visit_expr(n.iterable);
    os << ",\"block\":";
    visit_block(*n.block);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const FnDef& n) {
    os << "{\"node\":\"FnDef\",\"name\":";
    (*this)(n.name);
    os << ",\"params\":[";
    for (size_t i = 0; i < n.params.size(); ++i) {
      os << "{\"name\":";
      (*this)(n.params[i].name);
      os << ",\"type\":\"" << json_escape(n.params[i].type) << "\"}";
      if (i != n.params.size() - 1) os << ",";
    }
    os << "],\"return_type\":";
    if (n.return_type) {
      os << "\"" << json_escape(*n.return_type) << "\"";
    } else {
      os << "null";
    }
    os << ",\"block\":";
    visit_block(*n.block);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const StepStmt& n) {
    os << "{\"node\":\"StepStmt\",\"name\":";
    (*this)(n.name);
    os << ",\"param\":{\"name\":";
    (*this)(n.param.name);
    os << ",\"type\":\"" << json_escape(n.param.type) << "\"},\"block\":";
    visit_block(*n.block);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const ReturnStmt& n) {
    os << "{\"node\":\"ReturnStmt\",\"value\":";
    visit_expr(n.value);
    os << ",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const BreakStmt& n) {
    os << "{\"node\":\"BreakStmt\",";
    write_loc(os, n.loc);
    os << "}";
  }

  void operator()(const ContinueStmt& n) {
    os << "{\"node\":\"ContinueStmt\",";
    write_loc(os, n.loc);
    os << "}";
  }

  void visit_block(const CodeBlock& n) {
    os << "{\"node\":\"CodeBlock\",\"statements\":[";
    for (size_t i = 0; i < n.statements.size(); ++i) {
      visit_stmt(n.statements[i]);
      if (i != n.statements.size() - 1) os << ",";
    }
    os << "],";
    write_loc(os, n.loc);
    os << "}";
  }
};

// NOTE: root object intentionally omits "node" — it uses "statements" directly.
// All child nodes use "node":"..." as their type key. If a top-level type key
// is needed for the canvas pipeline, add "node":"Program" here.
std::string to_json(const Program& program) {
  std::ostringstream oss;
  oss << "{\"statements\":[";
  ToJsonVisitor visitor{oss};
  for (size_t i = 0; i < program.statements.size(); ++i) {
    visitor.visit_stmt(program.statements[i]);
    if (i != program.statements.size() - 1) oss << ",";
  }
  oss << "]}";
  return oss.str();
}

}  // namespace whis::ast
