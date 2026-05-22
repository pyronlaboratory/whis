#include <cassert>
#include <iostream>

#include "../test_helpers.hpp"
#include "whis/ast.hpp"
#include "whis/compiler.hpp"

int main() {
  print_suite_header("Whis AST Pipeline Integrity");
  print_case_start("AST Round-trip Verification");

  std::string seed_source =
      "let target_mass = 4.2 mg;\n"
      "enable gravity(9.81 m/s^2);\n";

  // Pass 1: Parse original code string into an AST structure
  auto parse_1 = whis::compiler::generate_ast(seed_source);
  WHIS_ASSERT(parse_1.success, "Failed to parse seed source tokens.");

  // Pass 2: Unparse the AST back into string format
  std::string unparsed_source = whis::compiler::generate_source(parse_1.ast);

  // Pass 3: Re-parse the unparsed code back into an alternate AST structure
  auto parse_2 = whis::compiler::generate_ast(unparsed_source);
  WHIS_ASSERT(
      parse_2.success,
      "Generated output failed to parse on round-trip verification pass.");

  // Structural Verification Check
  WHIS_ASSERT(parse_1.ast.statements.size() == parse_2.ast.statements.size(),
              "AST structural size changed during serialization loop.");

  print_case_success();
  return 0;
}