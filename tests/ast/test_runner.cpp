// Forward declarations
extern void run_ast_integrity_tests();
extern void run_ast_serialization_tests();
extern void run_dimensions_tests();

int main() {
  run_ast_integrity_tests();
  run_ast_serialization_tests();
  run_dimensions_tests();
  return 0;
}