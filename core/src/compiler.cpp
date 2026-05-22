#include "whis/compiler.hpp"

#include <iostream>
#include <tao/pegtl.hpp>

#include "whis/ast.hpp"
#include "whis/grammar.hpp"

namespace whis::compiler {

template <typename Rule>
struct Action : tao::pegtl::nothing<Rule> {};

template <>
struct Action<whis::grammar::Statement> {
  template <typename Input>
  static void apply(const Input& in, whis::ast::Program& program) {
    program.statements.push_back({in.string()});
  }
};

ParseResult generate_ast(const std::string& source) {
  if (source.empty()) {
    return {false, "Empty source input buffer", 1, 1, {}};
  }

  tao::pegtl::string_input<> input(source, "whis_spec_target");
  whis::ast::Program program;

  try {
    bool ok = tao::pegtl::parse<whis::grammar::Program, Action>(input, program);

    if (!ok) {
      // The parser cleanly returned false instead of throwing.
      // Let's grab the current iterator position to see how far it got!
      return {false,
              "Grammar structure mismatched top-level rules (returned false)",
              input.position().line, input.position().column, program};
    }

    return {true, "", 0, 0, program};
  } catch (const tao::pegtl::parse_error& e) {
    size_t error_line = 1;
    size_t error_col = 1;
    if (!e.positions().empty()) {
      const auto pos = e.positions().front();
      error_line = pos.line;
      error_col = pos.column;
    }
    return {false, e.what(), error_line, error_col, program};
  }
}

std::string generate_source(const whis::ast::Program& ast) {
  std::string output;
  for (const auto& stmt : ast.statements) {
    output += stmt.raw_content + ";\n";
  }
  return output;
}
}