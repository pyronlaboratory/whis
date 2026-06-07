// Forward declarations
extern void run_ast_integrity_tests();
extern void run_ast_serialization_tests();

int main() {
  run_ast_integrity_tests();
  run_ast_serialization_tests();
  return 0;
}