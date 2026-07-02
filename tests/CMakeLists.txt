# Global test configuration and helper library
include(CTest)

#-----------------------------------------------------------------------------
# Parser Test Target
#-----------------------------------------------------------------------------
add_executable(whis_parser_tests
    parser/test_runner.cpp
    parser/test_grammar_rules.cpp
    parser/test_lexical_tokens.cpp
)
target_link_libraries(whis_parser_tests PRIVATE whis_core)
add_test(NAME Whis.ParserTests COMMAND whis_parser_tests)

#-----------------------------------------------------------------------------
# AST Test Target
#-----------------------------------------------------------------------------
add_executable(whis_ast_tests
    ast/test_runner.cpp
    ast/test_ast_integrity.cpp
    ast/test_ast_serialization.cpp
    ast/test_semantic_types.cpp
)
target_link_libraries(whis_ast_tests PRIVATE whis_core)
add_test(NAME Whis.AstTests COMMAND whis_ast_tests)