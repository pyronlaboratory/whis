#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../test_helpers.hpp"
#include "whis/compiler.hpp"

struct LexicalTestCase {
  std::string description;
  std::string source_code;
  bool should_pass;
};

void run_lexical_token_tests() {
  print_suite_header("Whis Lexical Tokens & Terminal Rules");

  const std::vector<LexicalTestCase> test_cases = {
      // §3.1
      {"Numeric Terminal: Standard Integer", "let x = 42;", true},

      {"Numeric Terminal: Standard Decimal Float", "let x = 4.2;", true},

      {"Numeric Terminal: Scientific Notation (Positive Exponent)",
       "let x = 4.2e6;", true},

      {"Numeric Terminal: Scientific Notation (Negative Exponent)",
       "let x = 4.2e-6;", true},

      {"Numeric Terminal: Exponent with Explicit Plus Sign", "let x = 900E+2;",
       true},

      {"Unit Suffix: Single symbol matching length (m)",
       "let structural_length = 45 m;", true},

      {"Unit Suffix: Compound scaling quotient rules (km/h)",
       "let aircraft_speed = 900 km/h;", true},

      {"Unit Suffix: Squared exponent tracking formulas (m/s^2)",
       "let acceleration = 9.81 m/s^2;", true},

      {"Unit Suffix: Complex multi-unit multiplication layout (N*m^2/kg^2)",
       "let G = 6.6743e-11 N*m^2/kg^2;", true},

      {"UTF-8 Compliance: Non-ASCII characters and Greek variables inside line "
       "comments",
       "// Checking delta Δx layout equations with alpha α and beta β "
       "indicators\n"
       "let drag = 0.5;",
       true},

      {"UTF-8 Compliance: Multi-byte emojis inside comments",
       "// Enforcing strict bounds checking 🚀\n"
       "enable gravity(9.81 m/s^2);",
       true},

      {"Keyword Isolation: Variable containing 'in' as a partial prefix string",
       "let initialize = 10 m;", true},

      {"Keyword Isolation: Variable containing 'create' and 'state' as "
       "substrings",
       "let create_state = 20 kg;", true},

      {"Keyword Isolation: Variable containing 'if' as a partial prefix string",
       "let if_condition_flag = 1;", true},

      {"Keyword Isolation: Variable containing 'step' as a partial prefix "
       "string",
       "let step_interval = 0.01 s;", true},

      {"Negative Check: Bare restricted keyword 'in' illegal as identifier "
       "name",
       "let in = 5 m;", false},

      {"Negative Check: Bare restricted keyword 'create' illegal as identifier "
       "name",
       "let create = 14 kg;", false},

      {"Negative Check: Bare restricted keyword 'state' illegal as identifier "
       "name",
       "let state = 0.5 m;", false}};

  for (const auto& tc : test_cases) {
    print_case_start(tc.description);
    auto result = whis::compiler::generate_ast(tc.source_code);

    if (tc.should_pass) {
      WHIS_ASSERT(result.success,
                  "Syntax unexpectedly rejected a valid configuration stream! "
                  "Error Log: " +
                      result.error_log);
    } else {
      WHIS_ASSERT(!result.success,
                  "Parser structural defect! Vulnerability accepted a reserved "
                  "keyword identifier string as a variable binding name.");
    }

    print_case_success();
  }
}