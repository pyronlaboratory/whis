#pragma once

#include <tao/pegtl.hpp>

namespace whis::grammar {
namespace pegtl = tao::pegtl;

// =========================================================================
// Forward Declarations & Recursion Helpers
// =========================================================================

struct Expression;
struct Statement;
struct CodeBlock;
struct ControlStmt;

struct ExpressionRef : pegtl::seq<Expression> {};
struct CodeBlockRef : pegtl::seq<CodeBlock> {};
struct ControlStmtRef : pegtl::seq<ControlStmt> {};

// =========================================================================
// 1. Whitespace, Comments, and Layout Rules
// =========================================================================

// Line comments start with // and consume up to the newline
struct LineComment
    : pegtl::seq<pegtl::two<'/'>,
                 pegtl::until<pegtl::at<pegtl::eolf>, pegtl::any>> {};

// Horizontal space (spaces and tabs)
struct HSpace : pegtl::one<' ', '\t'> {};

// Ignored horizontal content
struct SpaceOrComment : pegtl::sor<HSpace, LineComment> {};

// Newlines (explicitly tracked because layout is newline-sensitive)
struct Newline : pegtl::one<'\r', '\n'> {};

// General spacing that does NOT cross a newline boundary
struct InlineSpace : pegtl::star<SpaceOrComment> {};

// Multi-line spacing used explicitly inside structural brackets where
// layout relaxes
struct BlockSpace : pegtl::star<pegtl::sor<pegtl::space, LineComment>> {};

// =========================================================================
// 2. Keywords and Identifiers
// =========================================================================

template <typename Key>
struct Keyword
    : pegtl::seq<Key,
                 pegtl::not_at<pegtl::sor<pegtl::alnum, pegtl::one<'_'>>>> {};

// clang-format off
struct K_create   : Keyword<TAO_PEGTL_STRING("create")> {};
struct K_let      : Keyword<TAO_PEGTL_STRING("let")> {};
struct K_enable   : Keyword<TAO_PEGTL_STRING("enable")> {};
struct K_disable  : Keyword<TAO_PEGTL_STRING("disable")> {};
struct K_show     : Keyword<TAO_PEGTL_STRING("show")> {};
struct K_hide     : Keyword<TAO_PEGTL_STRING("hide")> {};
struct K_if       : Keyword<TAO_PEGTL_STRING("if")> {};
struct K_else     : Keyword<TAO_PEGTL_STRING("else")> {};
struct K_while    : Keyword<TAO_PEGTL_STRING("while")> {};
struct K_for      : Keyword<TAO_PEGTL_STRING("for")> {};
struct K_in       : Keyword<TAO_PEGTL_STRING("in")> {};
struct K_fn       : Keyword<TAO_PEGTL_STRING("fn")> {};
struct K_step     : Keyword<TAO_PEGTL_STRING("step")> {};
struct K_state    : Keyword<TAO_PEGTL_STRING("state")> {};
struct K_return   : Keyword<TAO_PEGTL_STRING("return")> {};
struct K_continue : Keyword<TAO_PEGTL_STRING("continue")> {};
struct K_break    : Keyword<TAO_PEGTL_STRING("break")> {};
struct K_on       : Keyword<TAO_PEGTL_STRING("on")> {};
struct K_tweak    : Keyword<TAO_PEGTL_STRING("tweak")> {};
struct K_range    : Keyword<TAO_PEGTL_STRING("range")> {};
// clang-format on

struct ReservedWords
    : pegtl::sor<K_create, K_let, K_enable, K_disable, K_show, K_hide, K_if,
                 K_else, K_while, K_for, K_in, K_fn, K_step, K_state, K_return,
                 K_continue, K_break, K_on, K_tweak, K_range> {};

struct Identifier
    : pegtl::seq<pegtl::not_at<ReservedWords>, pegtl::identifier> {};

// =========================================================================
// 3. Literals & Primitives
// =========================================================================

// Numeric literals supporting scientific notation
struct Sign : pegtl::one<'+', '-'> {};
struct Exponent : pegtl::seq<pegtl::one<'e', 'E'>, pegtl::opt<Sign>,
                             pegtl::plus<pegtl::digit>> {};
struct FractionalPart : pegtl::seq<pegtl::one<'.'>, pegtl::plus<pegtl::digit>> {
};

struct NumericLiteral
    : pegtl::seq<pegtl::plus<pegtl::digit>, pegtl::opt<FractionalPart>,
                 pegtl::opt<Exponent>> {};

// Vector literals supporting 2D and 3D shapes
struct VectorLiteral
    : pegtl::seq<pegtl::one<'('>, BlockSpace, ExpressionRef, BlockSpace,
                 pegtl::one<','>, BlockSpace, ExpressionRef, BlockSpace,
                 pegtl::opt<pegtl::seq<pegtl::one<','>, BlockSpace,
                                       ExpressionRef, BlockSpace>>,
                 pegtl::one<')'>> {};

// =========================================================================
// 4. Dimensional Suffix Matrix (§3.1 Spec)
// =========================================================================

// PEG sor<> matches greedily without backtracking.
// Longest prefixes (e.g., "ms", "mg") must precede shorter versions ("s", "g")
// to prevent premature matching before BaseUnit's lookahead check.
struct UnitAtom
    : pegtl::sor<
          TAO_PEGTL_STRING("mm"), TAO_PEGTL_STRING("cm"),
          TAO_PEGTL_STRING("km"), TAO_PEGTL_STRING("mg"),
          TAO_PEGTL_STRING("kg"), TAO_PEGTL_STRING("min"),
          TAO_PEGTL_STRING("ms"), TAO_PEGTL_STRING("m"), TAO_PEGTL_STRING("g"),
          TAO_PEGTL_STRING("s"), TAO_PEGTL_STRING("h"), TAO_PEGTL_STRING("Pa"),
          TAO_PEGTL_STRING("rad"), TAO_PEGTL_STRING("deg"),
          TAO_PEGTL_STRING("N"), TAO_PEGTL_STRING("J"), TAO_PEGTL_STRING("W"),
          TAO_PEGTL_STRING("V"), TAO_PEGTL_STRING("A"), TAO_PEGTL_STRING("C")> {
};

// Strict isolation lookahead rule ensures physical units are cleanly
// distinguished from normal variable identifiers
struct BaseUnit : pegtl::seq<UnitAtom, pegtl::not_at<pegtl::identifier_other>> {
};

// Explicitly isolates structural dimensional exponents (like m/s^2) inside the
// suffix sub-tree to prevent them from leaking into the scalar mathematical
// expression parser.
struct SuffixTerm
    : pegtl::seq<BaseUnit,
                 pegtl::opt<pegtl::seq<pegtl::one<'^'>, pegtl::opt<Sign>,
                                       NumericLiteral>>> {};
struct SuffixMul
    : pegtl::seq<SuffixTerm,
                 pegtl::star<pegtl::seq<pegtl::one<'*'>, SuffixTerm>>> {};
struct Suffix : pegtl::seq<SuffixMul,
                           pegtl::opt<pegtl::seq<pegtl::one<'/'>, SuffixMul>>> {
};

// =========================================================================
// 5. Expression Grammar Hierarchy
// =========================================================================

// Parses dot-notation (obj.field) as a single primitive block.
// Prevents trailing sub-fields from short-circuiting at the identifier
// boundary.
struct DottedName
    : pegtl::seq<pegtl::not_at<ReservedWords>, pegtl::identifier,
                 pegtl::star<pegtl::seq<pegtl::one<'.'>, pegtl::identifier>>> {
};

struct DottedCall
    : pegtl::seq<DottedName, InlineSpace, pegtl::one<'('>, BlockSpace,
                 pegtl::opt<pegtl::list<ExpressionRef, pegtl::one<','>,
                                        SpaceOrComment>>,
                 BlockSpace, pegtl::one<')'>> {};

struct TweakExpr
    : pegtl::seq<K_tweak, InlineSpace, pegtl::one<'('>, BlockSpace,
                 ExpressionRef, BlockSpace, pegtl::one<','>, InlineSpace,
                 K_range, InlineSpace, pegtl::one<':'>, BlockSpace,
                 ExpressionRef, BlockSpace, TAO_PEGTL_STRING(".."), BlockSpace,
                 ExpressionRef, BlockSpace, pegtl::one<')'>> {};

struct Primary
    : pegtl::sor<
          VectorLiteral, NumericLiteral,
          // Evaluates parameterized runtime parameter modulations prior to
          // evaluating identifiers to protect downstream keyword symbols from
          // being prematurely consumed as generic object names.
          TweakExpr,
          // Evaluates compound calls (obj.fn()) before simple paths (obj.field)
          // to prevent the name sequence from consuming the prefix greedily.
          DottedCall, DottedName,
          pegtl::seq<pegtl::one<'('>, BlockSpace, ExpressionRef, BlockSpace,
                     pegtl::one<')'>>> {};

// Binds an optional leading sign, exponential power, and dimensional
// unit matrix directly to a primary token to parse a unified scalar factor.
struct Factor : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, Primary,
                           pegtl::opt<pegtl::seq<InlineSpace, Suffix>>,
                           pegtl::opt<pegtl::seq<InlineSpace, pegtl::one<'^'>,
                                                 InlineSpace, Primary>>> {};

struct OpMulDiv : pegtl::sor<pegtl::one<'*'>, pegtl::one<'/'>> {};
struct Term : pegtl::list<Factor, OpMulDiv, SpaceOrComment> {};

struct OpAddSub : pegtl::sor<pegtl::one<'+'>, pegtl::one<'-'>> {};

// Parses relational operators for conditional statements.
// Integrates lookahead matches before bare symbols to capture complete boolean
// predicates.
struct OpCmp : pegtl::sor<TAO_PEGTL_STRING("<="), TAO_PEGTL_STRING(">="),
                          TAO_PEGTL_STRING("=="), TAO_PEGTL_STRING("!="),
                          pegtl::one<'<'>, pegtl::one<'>'>> {};

// Structures expressions as arithmetic trees followed by an optional,
// non-associative comparison to isolate math operations from conditional
// evaluations.
struct ArithExpr : pegtl::list<Term, OpAddSub, SpaceOrComment> {};
struct Expression
    : pegtl::seq<
          ArithExpr,
          pegtl::opt<pegtl::seq<InlineSpace, OpCmp, InlineSpace, ArithExpr>>> {
};

struct ExpressionList
    : pegtl::list<ExpressionRef, pegtl::one<','>, SpaceOrComment> {};

// =========================================================================
// 6. Statement Declarations (§3.2 Spec)
// =========================================================================

struct PropertyPair : pegtl::seq<Identifier, InlineSpace, pegtl::one<':'>,
                                 BlockSpace, ExpressionRef> {};

// Explicitly handles multi-line property fields using vertical spacing layout
// rules. Overrides default horizontal list padding to safely consume trailing
// carriage returns between key-value elements without breaking statement
// blocks.
struct PropSeparator
    : pegtl::seq<BlockSpace,
                 pegtl::sor<pegtl::one<','>, pegtl::one<';'>, Newline>,
                 BlockSpace> {};

struct PropertyList : pegtl::list<PropertyPair, PropSeparator> {};

// 6.1 Create Statement
struct CreateStmt
    : pegtl::seq<K_create, InlineSpace, Identifier, InlineSpace,
                 pegtl::one<'{'>, BlockSpace, pegtl::opt<PropertyList>,
                 BlockSpace,
                 pegtl::opt<pegtl::sor<pegtl::one<','>, pegtl::one<';'>>>,
                 BlockSpace, pegtl::one<'}'>> {};

// 6.2 Let Statement
struct LetStmt : pegtl::seq<K_let, InlineSpace, Identifier,
                            pegtl::opt<pegtl::seq<InlineSpace, pegtl::one<'='>,
                                                  BlockSpace, ExpressionRef>>> {
};

// 6.3 Enable / Disable Modifier Statement
struct EnableStmt
    : pegtl::seq<
          pegtl::sor<K_enable, K_disable>, InlineSpace, Identifier,
          pegtl::opt<pegtl::seq<
              InlineSpace, pegtl::one<'('>, BlockSpace,
              pegtl::opt<pegtl::list<pegtl::sor<PropertyPair, ExpressionRef>,
                                     pegtl::one<','>, SpaceOrComment>>,
              BlockSpace, pegtl::one<')'>>>> {};

// 6.4 Show / Hide Instrumentation Statement
struct ShowStmt
    : pegtl::seq<
          pegtl::sor<K_show, K_hide>, InlineSpace, Identifier, InlineSpace,
          pegtl::opt<pegtl::seq<pegtl::one<'('>, BlockSpace, ExpressionList,
                                BlockSpace, pegtl::one<')'>, InlineSpace>>,
          pegtl::opt<pegtl::seq<K_on, InlineSpace, Identifier, InlineSpace>>,
          pegtl::opt<pegtl::seq<
              Identifier, InlineSpace, pegtl::one<'('>, BlockSpace,
              pegtl::list<Identifier, pegtl::one<'x'>, SpaceOrComment>,
              BlockSpace, pegtl::one<')'>>>> {};

// 6.5 Assignment Statement
struct AssignStmt : pegtl::seq<DottedName, InlineSpace, pegtl::one<'='>,
                               BlockSpace, ExpressionRef> {};

// 6.6 Function Parameters
struct TypeAnnotation
    : pegtl::seq<pegtl::opt<TAO_PEGTL_STRING("->")>, InlineSpace,
                 pegtl::sor<BaseUnit, Identifier>> {};
struct ParamPair : pegtl::seq<Identifier, InlineSpace, pegtl::one<':'>,
                              InlineSpace, pegtl::sor<BaseUnit, Identifier>> {};

// 6.7 Control Flow
struct ControlStmt
    : pegtl::sor<
          // While block
          pegtl::seq<K_while, InlineSpace, pegtl::one<'('>, BlockSpace,
                     ExpressionRef, BlockSpace, pegtl::one<')'>, BlockSpace,
                     CodeBlockRef>,

          // If-Else block
          pegtl::seq<
              K_if, InlineSpace, pegtl::one<'('>, BlockSpace, ExpressionRef,
              BlockSpace, pegtl::one<')'>, BlockSpace, CodeBlockRef,
              pegtl::opt<pegtl::seq<BlockSpace, K_else, BlockSpace,
                                    pegtl::sor<CodeBlockRef, ControlStmtRef>>>>,

          // For iteration block
          pegtl::seq<K_for, InlineSpace, Identifier, InlineSpace, K_in,
                     InlineSpace, ExpressionRef, BlockSpace, CodeBlockRef>,

          // Function definition block
          pegtl::seq<K_fn, InlineSpace, Identifier, InlineSpace,
                     pegtl::one<'('>, BlockSpace,
                     pegtl::list<ParamPair, pegtl::one<','>, SpaceOrComment>,
                     BlockSpace, pegtl::one<')'>, InlineSpace,
                     pegtl::opt<pegtl::seq<TAO_PEGTL_STRING("->"), InlineSpace,
                                           pegtl::sor<BaseUnit, Identifier>,
                                           InlineSpace>>,
                     CodeBlockRef>,

          // Custom solver block
          pegtl::seq<K_step, InlineSpace, Identifier, InlineSpace,
                     pegtl::one<'('>, BlockSpace, ParamPair, BlockSpace,
                     pegtl::one<')'>, BlockSpace, CodeBlockRef>,

          // Return, break or continue block
          // Resolves statements with dynamic terminal boundaries first.
          // Evaluates the mandatory trailing expression tree of return blocks
          // before checking for naked termination tokens.
          pegtl::seq<K_return, InlineSpace, ExpressionRef>, K_break,
          K_continue> {};

// 6.8 Statement Aggregation
struct Statement : pegtl::sor<CreateStmt, LetStmt, EnableStmt, ShowStmt,
                              ControlStmt, AssignStmt> {};

// =========================================================================
// 6.9 Separation and Layout Controls
// =========================================================================

struct InlineSeparator : pegtl::seq<InlineSpace, pegtl::one<';'>> {};
struct BlockSeparator
    : pegtl::seq<InlineSpace,
                 pegtl::sor<pegtl::one<';'>, Newline, pegtl::eof>> {};

struct CodeBlock
    : pegtl::seq<pegtl::one<'{'>, BlockSpace,
                 pegtl::star<pegtl::seq<Statement, BlockSeparator, BlockSpace>>,
                 pegtl::one<'}'>> {};

// =========================================================================
// 7. Global Program Entrance Rule
// =========================================================================
struct Program
    : pegtl::seq<BlockSpace,
                 pegtl::star<pegtl::seq<Statement, BlockSeparator, BlockSpace>>,
                 pegtl::opt<Statement>, pegtl::eof> {};

}  // namespace whis::grammar