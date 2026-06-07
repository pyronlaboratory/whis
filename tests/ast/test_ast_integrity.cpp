#include <cassert>
#include <iostream>
#include <string>

#include "../test_helpers.hpp"
#include "whis/ast.hpp"
#include "whis/compiler.hpp"

void run_ast_integrity_tests() {
  print_suite_header("Whis AST Pipeline Integrity");

  // §AC.1  Source location: line offsets are preserved on leaf nodes
  {
    print_case_start("Source Mapping: let on line 1 carries loc.line == 1");
    auto result = whis::compiler::generate_ast("let x = 42;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    WHIS_ASSERT(!result.ast.statements.empty(), "Expected 1 statement");
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.loc.line == 1,
                "Expected line 1, got " + std::to_string(stmt.loc.line));
    print_case_success();
  }

  {
    print_case_start("Source Mapping: second statement carries loc.line == 2");
    auto result = whis::compiler::generate_ast("let a = 1;\nlet b = 2;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    WHIS_ASSERT(result.ast.statements.size() == 2, "Expected 2 statements");
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[1].data);
    WHIS_ASSERT(stmt.loc.line == 2,
                "Expected line 2, got " + std::to_string(stmt.loc.line));
    print_case_success();
  }

  // §AC.2  Structural integrity: 7-element dimension array
  {
    print_case_start(
        "Structural Integrity: DimensionedValue has exactly 7 dimensions");
    auto result = whis::compiler::generate_ast("let x = 9.81;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.value.has_value(), "LetStmt missing init value");
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    WHIS_ASSERT(
        dv.dimensions.size() == 7,
        "Expected 7 dimensions, got " + std::to_string(dv.dimensions.size()));
    print_case_success();
  }

  {
    print_case_start(
        "Structural Integrity: 45 cm normalises to 0.45 (SI baseline)");
    auto result = whis::compiler::generate_ast("let x = 45 cm;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    WHIS_ASSERT(std::abs(dv.value - 0.45) < 1e-9,
                "Expected 0.45, got " + std::to_string(dv.value));
    const whis::ast::DimArray expected = {0, 1, 0, 0, 0, 0, 0};
    WHIS_ASSERT(dv.dimensions == expected,
                "Dimension array mismatch for cm (length)");
    print_case_success();
  }

  {
    print_case_start("Structural Integrity: 4.2 mg normalises to 4.2e-6 kg");
    auto result = whis::compiler::generate_ast("let m = 4.2 mg;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    WHIS_ASSERT(std::abs(dv.value - 4.2e-6) < 1e-20,
                "Expected 4.2e-6, got " + std::to_string(dv.value));
    const whis::ast::DimArray expected = {1, 0, 0, 0, 0, 0, 0};
    WHIS_ASSERT(dv.dimensions == expected,
                "Dimension array mismatch for mg (mass)");
    print_case_success();
  }

  {
    print_case_start(
        "Structural Integrity: Newton gives dims [1,1,-2,0,0,0,0]");
    auto result = whis::compiler::generate_ast("let f = 5 N;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    const whis::ast::DimArray expected = {1, 1, -2, 0, 0, 0, 0};
    WHIS_ASSERT(dv.dimensions == expected,
                "Dimension array mismatch for N — check negative exponent");
    print_case_success();
  }

  {
    print_case_start(
        "Structural Integrity: negative exponent stored without sign "
        "truncation");
    // T exponent of Newton is -2; int16_t must preserve it exactly.
    auto result = whis::compiler::generate_ast("let f = 1 N;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    const int16_t t_exp = dv.dimensions[2];  // index 2 = T
    WHIS_ASSERT(t_exp == -2, "Expected T exponent -2 for Newton, got " +
                                 std::to_string(t_exp));
    print_case_success();
  }

  {
    print_case_start(
        "Structural Integrity: compound suffix m/s^2 gives dims "
        "[0,1,-2,0,0,0,0]");
    auto result = whis::compiler::generate_ast("let a = 9.81 m/s^2;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    const whis::ast::DimArray expected = {0, 1, -2, 0, 0, 0, 0};
    WHIS_ASSERT(dv.dimensions == expected,
                "Dimension array mismatch for m/s^2 (acceleration)");
    print_case_success();
  }

  // §AC.3  Statement variant topology
  {
    print_case_start("Spec Alignment: create produces CreateStmt variant");
    auto result = whis::compiler::generate_ast("create Ball { mass: 1 }");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    WHIS_ASSERT(std::holds_alternative<whis::ast::CreateStmt>(
                    result.ast.statements[0].data),
                "Expected CreateStmt");
    print_case_success();
  }

  {
    print_case_start("Spec Alignment: let produces LetStmt variant");
    auto result = whis::compiler::generate_ast("let speed = 30;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    WHIS_ASSERT(std::holds_alternative<whis::ast::LetStmt>(
                    result.ast.statements[0].data),
                "Expected LetStmt");
    print_case_success();
  }

  {
    print_case_start(
        "Spec Alignment: enable produces EnableStmt (is_enable=true)");
    auto result = whis::compiler::generate_ast("enable gravity(9.81);");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::EnableStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.is_enable == true, "Expected is_enable=true");
    print_case_success();
  }

  {
    print_case_start(
        "Spec Alignment: disable produces EnableStmt (is_enable=false)");
    auto result = whis::compiler::generate_ast("disable gravity;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::EnableStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.is_enable == false, "Expected is_enable=false");
    print_case_success();
  }

  {
    print_case_start("Spec Alignment: assignment produces AssignStmt variant");
    auto result =
        whis::compiler::generate_ast("obj.velocity = obj.velocity + 1;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    WHIS_ASSERT(std::holds_alternative<whis::ast::AssignStmt>(
                    result.ast.statements[0].data),
                "Expected AssignStmt");
    print_case_success();
  }

  {
    print_case_start(
        "Spec Alignment: break produces ControlStmt with BreakStmt");
    auto result = whis::compiler::generate_ast("while (x < 10) { break; }");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& ctrl =
        std::get<whis::ast::ControlStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(std::holds_alternative<whis::ast::BreakStmt>(ctrl.data),
                "Expected BreakStmt inside ControlStmt");
    print_case_success();
  }

  // §AC.4  LetStmt name and value captured correctly
  {
    print_case_start("Spec Alignment: let captures variable name correctly");
    auto result = whis::compiler::generate_ast("let target_mass = 4.2;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.name.name == "target_mass",
                "Expected 'target_mass', got '" + stmt.name.name + "'");
    print_case_success();
  }

  {
    print_case_start("Spec Alignment: uninitialised let has value == nullopt");
    auto result = whis::compiler::generate_ast("let x;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(!stmt.value.has_value(),
                "Uninitialised let should have value == nullopt");
    print_case_success();
  }

  // §AC.5  CreateStmt property count
  {
    print_case_start("Spec Alignment: create captures correct property count");
    auto result = whis::compiler::generate_ast(
        "create Ball {\n"
        "    mass: 1,\n"
        "    radius: 0.5,\n"
        "    charge: 0.0\n"
        "}");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::CreateStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(
        stmt.properties.size() == 3,
        "Expected 3 properties, got " + std::to_string(stmt.properties.size()));
    print_case_success();
  }

  // §AC.6  VectorLiteral shape
  {
    print_case_start("Spec Alignment: 2D VectorLiteral has 2 elements");
    auto result = whis::compiler::generate_ast("let pos = (0, 25);");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    WHIS_ASSERT(stmt.value.has_value(), "LetStmt missing init value");
    const auto& vl = std::get<whis::ast::VectorLiteral>(stmt.value->data);
    WHIS_ASSERT(
        vl.elements.size() == 2,
        "Expected 2 elements, got " + std::to_string(vl.elements.size()));
    print_case_success();
  }

  {
    print_case_start("Spec Alignment: 3D VectorLiteral has 3 elements");
    auto result = whis::compiler::generate_ast("let v3 = (1, 2, 3);");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& vl = std::get<whis::ast::VectorLiteral>(stmt.value->data);
    WHIS_ASSERT(
        vl.elements.size() == 3,
        "Expected 3 elements, got " + std::to_string(vl.elements.size()));
    print_case_success();
  }

  // §AC.7  Round-trip
  {
    print_case_start(
        "Round-trip: statement count and types survive unparse/reparse");
    const std::string seed =
        "let target_mass = 4.2;\n"
        "enable gravity(9.81);\n";

    auto pass1 = whis::compiler::generate_ast(seed);
    WHIS_ASSERT(pass1.success, "Pass 1 parse failed: " + pass1.error_log);

    const std::string unparsed = whis::compiler::generate_source(pass1.ast);

    auto pass2 = whis::compiler::generate_ast(unparsed);
    WHIS_ASSERT(pass2.success,
                "Pass 2 parse failed on unparsed source: " + pass2.error_log);

    WHIS_ASSERT(pass1.ast.statements.size() == pass2.ast.statements.size(),
                "Statement count changed across round-trip");

    // Verify statement types are preserved, not just count.
    for (size_t i = 0; i < pass1.ast.statements.size(); ++i) {
      WHIS_ASSERT(pass1.ast.statements[i].data.index() ==
                      pass2.ast.statements[i].data.index(),
                  "Statement type at index " + std::to_string(i) +
                      " changed across round-trip");
    }
    print_case_success();
  }
}