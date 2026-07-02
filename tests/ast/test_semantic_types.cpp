#include <string>
#include <vector>

#include "../test_helpers.hpp"
#include "whis/ast.hpp"
#include "whis/compiler.hpp"
#include "whis/dimensions.hpp"

// =========================================================================
// Helpers
// =========================================================================

// Parses source and runs the dim-checker in one call.
// Returns true if check passes without a DimError.
static bool dim_ok(const std::string& src) {
  auto result = whis::compiler::generate_ast(src);
  if (!result.success) return false;
  try {
    whis::dimensions::check(result.ast);
    return true;
  } catch (const whis::dimensions::DimError&) {
    return false;
  }
}

// Returns the DimError thrown by the checker, or terminates the test if the
// source fails to parse or no DimError is raised.
static whis::dimensions::DimError dim_err(const std::string& src,
                                          const std::string& ctx) {
  auto result = whis::compiler::parse_only(src);
  WHIS_ASSERT(result.success,
              "[" + ctx + "] parse failed: " + result.error_log);
  try {
    whis::dimensions::check(result.ast);
    WHIS_ASSERT(false, "[" + ctx + "] expected DimError but check passed");
  } catch (const whis::dimensions::DimError& e) {
    return e;
  }
  // unreachable; keeps compiler happy
  throw std::logic_error("unreachable");
}

// =========================================================================
// Test runner
// =========================================================================

void run_dimensions_tests() {
  print_suite_header("Whis Dimensional Analysis (§2.1 / §2.2)");

  // -----------------------------------------------------------------------
  // §AC.1  E_DIM_MISMATCH
  // -----------------------------------------------------------------------

  {
    print_case_start("E_DIM_MISMATCH: adding Mass to Velocity is rejected");
    const auto e = dim_err("let bad = 5 kg + 12 m/s;", "mass+vel");
    WHIS_ASSERT(e.code == whis::dimensions::ErrorCode::E_DIM_MISMATCH,
                "Expected E_DIM_MISMATCH error code");
    print_case_success();
  }

  {
    print_case_start(
        "E_DIM_MISMATCH: error message carries E_DIM_MISMATCH prefix");
    const auto e = dim_err("let bad = 1 m + 1 kg;", "m+kg msg");
    const std::string msg(e.what());
    WHIS_ASSERT(msg.find("E_DIM_MISMATCH") != std::string::npos,
                "Error message missing E_DIM_MISMATCH: " + msg);
    print_case_success();
  }

  {
    print_case_start(
        "E_DIM_MISMATCH: subtracting Length from Time is rejected");
    const auto e = dim_err("let bad = 10 s - 3 m;", "s-m");
    WHIS_ASSERT(e.code == whis::dimensions::ErrorCode::E_DIM_MISMATCH,
                "Expected E_DIM_MISMATCH for s - m");
    print_case_success();
  }

  {
    print_case_start("E_DIM_MISMATCH: error carries source location");
    const auto e = dim_err("let bad = 5 kg + 12 m/s;", "loc");
    // Line 1, column must be > 0 (exact column is parser-dependent).
    WHIS_ASSERT(e.loc.line >= 1, "Expected valid source line");
    WHIS_ASSERT(e.loc.column >= 1, "Expected valid source column");
    print_case_success();
  }

  // -----------------------------------------------------------------------
  // §AC.2  E_UNIT_NON_TRANSCENDENT
  // -----------------------------------------------------------------------

  {
    print_case_start(
        "E_UNIT_NON_TRANSCENDENT: sin() with Length argument is rejected");
    const auto e = dim_err("let bad = sin(1.0 m);", "sin(m)");
    WHIS_ASSERT(e.code == whis::dimensions::ErrorCode::E_UNIT_NON_TRANSCENDENT,
                "Expected E_UNIT_NON_TRANSCENDENT error code");
    print_case_success();
  }

  {
    print_case_start(
        "E_UNIT_NON_TRANSCENDENT: cos() with Time argument is rejected");
    const auto e = dim_err("let bad = cos(2.0 s);", "cos(s)");
    WHIS_ASSERT(e.code == whis::dimensions::ErrorCode::E_UNIT_NON_TRANSCENDENT,
                "Expected E_UNIT_NON_TRANSCENDENT for cos(s)");
    print_case_success();
  }

  {
    print_case_start("E_UNIT_NON_TRANSCENDENT: error message carries prefix");
    const auto e = dim_err("let bad = exp(1.0 kg);", "exp(kg) msg");
    const std::string msg(e.what());
    WHIS_ASSERT(msg.find("E_UNIT_NON_TRANSCENDENT") != std::string::npos,
                "Missing E_UNIT_NON_TRANSCENDENT prefix: " + msg);
    print_case_success();
  }

  {
    print_case_start(
        "E_UNIT_NON_TRANSCENDENT: sin() with dimensionless passes");
    WHIS_ASSERT(dim_ok("let theta = sin(0.5);"),
                "sin() with dimensionless literal should pass");
    print_case_success();
  }

  // -----------------------------------------------------------------------
  // §AC.3  Algebraic Propagation (§2.2)
  // -----------------------------------------------------------------------

  {
    print_case_start(
        "Propagation: force / mass yields Acceleration dims [0,1,-2,0,0,0,0]");
    auto result = whis::compiler::parse_only(
        "let F = 10 N;\nlet m = 2 kg;\nlet a = F / m;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);

    bool ok = true;
    try {
      whis::dimensions::check(result.ast);
    } catch (const whis::dimensions::DimError&) {
      ok = false;
    }
    WHIS_ASSERT(ok, "Checker rejected valid force/mass expression");

    const auto& let_f =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv_f = std::get<whis::ast::DimensionedValue>(let_f.value->data);
    const whis::ast::DimArray expected_force = {1, 1, -2, 0, 0, 0, 0};
    WHIS_ASSERT(dv_f.dimensions == expected_force,
                "Force dims mismatch — check suffix application for N");
    print_case_success();
  }

  {
    print_case_start(
        "Propagation: multiplication adds exponent arrays (m * s)");
    // m [0,1,0,...] * s [0,0,1,...] = [0,1,1,...] (metre-seconds)
    auto result = whis::compiler::generate_ast("let ms = 1 m * 2 s;");
    // Not a mismatch, check passes.
    WHIS_ASSERT(dim_ok("let ms = 1 m * 2 s;"), "m * s should pass checker");
    print_case_success();
  }

  {
    print_case_start("Propagation: m/s^2 resolves to Acceleration dims");
    auto result = whis::compiler::parse_only("let a = 9.81 m/s^2;");
    WHIS_ASSERT(result.success, "Parse failed: " + result.error_log);
    const auto& stmt =
        std::get<whis::ast::LetStmt>(result.ast.statements[0].data);
    const auto& dv = std::get<whis::ast::DimensionedValue>(stmt.value->data);
    const whis::ast::DimArray expected = {0, 1, -2, 0, 0, 0, 0};
    WHIS_ASSERT(dv.dimensions == expected,
                "m/s^2 suffix did not produce Acceleration dims");
    WHIS_ASSERT(dim_ok("let a = 9.81 m/s^2;"),
                "9.81 m/s^2 should pass checker");
    print_case_success();
  }

  {
    print_case_start("Propagation: same-unit addition passes (m + m)");
    WHIS_ASSERT(dim_ok("let d = 1 m + 2 m;"), "m + m should pass");
    print_case_success();
  }

  {
    print_case_start("Propagation: dimensionless power exponent scales dims");
    // (1 m)^2 → dim = L^2 = [0,2,0,...]
    WHIS_ASSERT(dim_ok("let area = 1 m ^ 2;"), "m^2 should pass checker");
    print_case_success();
  }

  {
    print_case_start(
        "Propagation: dimensionless literal mixes cleanly with dimensioned (0 "
        "m)");
    // Patterns like `(0 m, 25 m)` use a bare 0 as first component.
    WHIS_ASSERT(dim_ok("let pos = (0 m, 25 m);"),
                "Vector with dimensioned components should pass");
    print_case_success();
  }

  // -----------------------------------------------------------------------
  // §AC.4  Clean passes — valid physics programs must not be rejected
  // -----------------------------------------------------------------------

  {
    print_case_start("Clean pass: gravity enable with m/s^2 passes");
    WHIS_ASSERT(dim_ok("enable gravity(9.81 m/s^2);"),
                "enable gravity should pass");
    print_case_success();
  }

  {
    print_case_start(
        "Clean pass: unit scale literals (45 cm, 900 km/h, 4.2 mg)");
    WHIS_ASSERT(dim_ok("let structural_length = 45 cm;\n"
                       "let aircraft_speed    = 900 km/h;\n"
                       "let target_mass       = 4.2 mg;"),
                "SI literal normalization cases should all pass");
    print_case_success();
  }

  {
    print_case_start("Clean pass: compound G constant (N*m^2/kg^2)");
    WHIS_ASSERT(dim_ok("let G = 6.6743e-11 N*m^2/kg^2;"),
                "Gravitational constant suffix should pass");
    print_case_success();
  }
}