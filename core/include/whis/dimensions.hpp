#pragma once

#include <stdexcept>
#include <string>

#include "whis/ast.hpp"

// =========================================================================
// §8 Compiler Pass: Compile-Time Dimensional Analysis (§2.1 / §2.2)
// =========================================================================
//
// This pass runs as a post-parse semantic validation step, traversing the
// AST bottom-up and verifying that all binary operations conform to the
// algebraic propagation rules defined in §2.2.
//
// Entry point: whis::dimensions::check(program)
//   - Returns normally on success.
//   - Throws DimError on the first violation encountered.
//
// Integration: call from whis::compiler::generate_ast after a successful
// PEGTL parse, before handing the AST to downstream passes.

namespace whis::dimensions {

// =========================================================================
// Error Classification (§9 Error Specification Matrix)
// =========================================================================

enum class ErrorCode {
  E_DIM_MISMATCH,           // add/sub operand dimensions differ
  E_UNIT_NON_TRANSCENDENT,  // transcendental fn received dimensioned argument
};

struct DimError : std::runtime_error {
  ErrorCode code;
  ast::SourceLocation loc;

  DimError(ErrorCode c, const ast::SourceLocation& l, const std::string& msg)
      : std::runtime_error(msg), code(c), loc(l) {}
};

// =========================================================================
// Compile-Time Dimension Constants (§2.1 Primary Derived Dimension Matrix)
// =========================================================================

// Indices: [M, L, T, I, Θ, N, J]
namespace dims {
// clang-format off
inline constexpr ast::DimArray kDimensionless  = {0,  0,  0, 0, 0, 0, 0};
inline constexpr ast::DimArray kLength         = {0,  1,  0, 0, 0, 0, 0};
inline constexpr ast::DimArray kMass           = {1,  0,  0, 0, 0, 0, 0};
inline constexpr ast::DimArray kTime           = {0,  0,  1, 0, 0, 0, 0};
inline constexpr ast::DimArray kVelocity       = {0,  1, -1, 0, 0, 0, 0};
inline constexpr ast::DimArray kAcceleration   = {0,  1, -2, 0, 0, 0, 0};
inline constexpr ast::DimArray kForce          = {1,  1, -2, 0, 0, 0, 0};
inline constexpr ast::DimArray kEnergy         = {1,  2, -2, 0, 0, 0, 0};
// clang-format on
}  // namespace dims

// =========================================================================
// Public Interface
// =========================================================================

/**
 * @brief Runs the dimensional analysis pass over a fully-parsed AST program.
 *
 * Traverses every expression bottom-up, computing inferred DimArrays and
 * validating §2.2 algebraic propagation rules. Throws DimError on the first
 * violation; the caller is responsible for catching and converting to a
 * compiler diagnostic.
 *
 * @param program  The AST produced by whis::compiler::generate_ast.
 * @throws DimError  E_DIM_MISMATCH or E_UNIT_NON_TRANSCENDENT.
 */
void check(const ast::Program& program);

}  // namespace whis::dimensions