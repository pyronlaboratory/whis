#include <iostream>
#include <string>
#include <vector>

#include "../test_helpers.hpp"
#include "whis/ast.hpp"
#include "whis/compiler.hpp"

static bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

static std::string json_of(const std::string& source, const std::string& ctx) {
  auto result = whis::compiler::generate_ast(source);
  WHIS_ASSERT(result.success,
              "[" + ctx + "] Parse failed: " + result.error_log);
  return whis::ast::to_json(result.ast);
}

void run_ast_serialization_tests() {
  print_suite_header("Whis AST Serialisation");

  // Schema structure
  {
    print_case_start("JSON Schema: output contains \"statements\" array");
    auto j = json_of("let x = 1;", "schema");
    WHIS_ASSERT(contains(j, "\"statements\""), "Missing statements array");
    print_case_success();
  }

  {
    print_case_start("JSON Schema: empty program produces valid JSON shell");
    auto result = whis::compiler::generate_ast("// empty\n");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto j = whis::ast::to_json(result.ast);
    WHIS_ASSERT(contains(j, "\"statements\""),
                "Empty program missing statements key");
    print_case_success();
  }

  // Node kind coverage — one source per statement type
  {
    print_case_start("JSON Node: LetStmt emits \"node\":\"LetStmt\"");
    WHIS_ASSERT(contains(json_of("let x = 42;", "let"), "\"node\":\"LetStmt\""),
                "Missing LetStmt node");
    print_case_success();
  }

  {
    print_case_start("JSON Node: CreateStmt emits \"node\":\"CreateStmt\"");
    WHIS_ASSERT(contains(json_of("create Ball { mass: 1 }", "create"),
                         "\"node\":\"CreateStmt\""),
                "Missing CreateStmt node");
    print_case_success();
  }

  {
    print_case_start("JSON Node: EnableStmt emits \"node\":\"EnableStmt\"");
    WHIS_ASSERT(contains(json_of("enable gravity(9.81);", "enable"),
                         "\"node\":\"EnableStmt\""),
                "Missing EnableStmt node");
    print_case_success();
  }

  {
    print_case_start("JSON Node: EnableStmt (disable) emits is_enable:false");
    WHIS_ASSERT(
        contains(json_of("disable gravity;", "disable"), "\"is_enable\":false"),
        "disable did not emit is_enable:false");
    print_case_success();
  }

  {
    print_case_start("JSON Node: AssignStmt emits \"node\":\"AssignStmt\"");
    WHIS_ASSERT(
        contains(json_of("obj.x = 1;", "assign"), "\"node\":\"AssignStmt\""),
        "Missing AssignStmt node");
    print_case_success();
  }

  {
    print_case_start("JSON Node: ShowStmt emits \"node\":\"ShowStmt\"");
    WHIS_ASSERT(
        contains(json_of("show vectors(velocity) on Projectile;", "show"),
                 "\"node\":\"ShowStmt\""),
        "Missing ShowStmt node");
    print_case_success();
  }

  {
    print_case_start("JSON Node: BreakStmt emits \"node\":\"BreakStmt\"");
    WHIS_ASSERT(contains(json_of("while (x < 10) { break; }", "break"),
                         "\"node\":\"BreakStmt\""),
                "Missing BreakStmt node");
    print_case_success();
  }

  {
    print_case_start(
        "JSON Node: VectorLiteral emits \"node\":\"VectorLiteral\"");
    WHIS_ASSERT(contains(json_of("let pos = (0, 25);", "vec"),
                         "\"node\":\"VectorLiteral\""),
                "Missing VectorLiteral node");
    print_case_success();
  }

  {
    print_case_start(
        "JSON Node: DimensionedValue emits 7-element dimensions array");
    const auto j = json_of("let x = 42;", "dims");
    WHIS_ASSERT(contains(j, "\"dimensions\""), "Missing dimensions key");

    const size_t start = j.find("\"dimensions\":[");
    WHIS_ASSERT(start != std::string::npos, "dimensions array not found");

    const size_t bracket = j.find('[', start);
    const size_t close = j.find(']', bracket);
    const std::string arr = j.substr(bracket + 1, close - bracket - 1);
    int commas = 0;
    for (char c : arr)
      if (c == ',') ++commas;
    WHIS_ASSERT(commas == 6, "Expected 6 commas (7 elements), got " +
                                 std::to_string(commas));
    print_case_success();
  }

  // Location fields
  {
    print_case_start(
        "JSON Location: loc object contains line and column fields");
    const auto j = json_of("let x = 1;", "loc");
    WHIS_ASSERT(contains(j, "\"line\""), "Missing line field");
    WHIS_ASSERT(contains(j, "\"column\""), "Missing column field");
    WHIS_ASSERT(contains(j, "\"file\""), "Missing file field");
    print_case_success();
  }

  {
    print_case_start("JSON Location: second statement serializes with line:2");
    auto result = whis::compiler::generate_ast("let a = 1;\nlet b = 2;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto j = whis::ast::to_json(result.ast);
    WHIS_ASSERT(contains(j, "\"line\":2"), "Second statement missing line:2");
    print_case_success();
  }

  // Structural correctness
  {
    print_case_start("JSON Structure: CreateStmt name field matches source");
    const auto j = json_of("create Projectile { mass: 1 }", "create_name");
    WHIS_ASSERT(contains(j, "\"Projectile\""), "Entity name missing from JSON");
    print_case_success();
  }

  {
    print_case_start("JSON Structure: LetStmt with no init emits value:null");
    const auto j = json_of("let x;", "let_null");
    WHIS_ASSERT(contains(j, "\"value\":null"),
                "Uninitialised let missing null value");
    print_case_success();
  }

  {
    print_case_start("JSON Structure: braces are balanced");
    const auto j = json_of("let x = 1;\nlet y = 2;", "balance");
    int depth = 0;
    for (char c : j) {
      if (c == '{') ++depth;
      if (c == '}') --depth;
      WHIS_ASSERT(depth >= 0, "Unmatched closing brace in JSON");
    }
    WHIS_ASSERT(depth == 0, "Unmatched opening brace in JSON");
    print_case_success();
  }
}