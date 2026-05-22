# Global test configuration and helper library
include(CTest)

#-----------------------------------------------------------------------------
# Frontend / Parser Test Target
#-----------------------------------------------------------------------------
add_executable(whis_parser_tests
    parser/test_grammar_rules.cpp
)
target_link_libraries(whis_parser_tests PRIVATE whis_core)
add_test(NAME Whis.ParserTests COMMAND whis_parser_tests)

#-----------------------------------------------------------------------------
# AST / Structural Test Target
#-----------------------------------------------------------------------------
add_executable(whis_ast_tests
    ast/test_ast_integrity.cpp
)
target_link_libraries(whis_ast_tests PRIVATE whis_core)
add_test(NAME Whis.AstTests COMMAND whis_ast_tests)