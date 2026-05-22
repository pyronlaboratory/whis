#pragma once

#include <string>

#include "whis/ast.hpp"

namespace whis::compiler {
struct ParseResult {
  bool success;
  std::string error_log;
  size_t line;
  size_t column;
  whis::ast::Program ast;
};

ParseResult generate_ast(const std::string& source);
std::string generate_source(const whis::ast::Program& ast);
}  // namespace whis::compiler
