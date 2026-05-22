#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

inline const std::string ANSI_RED = "\033[31m";
inline const std::string ANSI_GREEN = "\033[32m";
inline const std::string ANSI_RESET = "\033[0m";

#define WHIS_ASSERT(condition, message)                              \
  do {                                                               \
    if (!(condition)) {                                              \
      std::cerr << "\n"                                              \
                << "  " << ANSI_RED << "[FAIL]" << ANSI_RESET << " " \
                << message << "\n\n";                                \
      std::exit(1);                                                  \
    }                                                                \
  } while (0)

inline void print_suite_header(const std::string& suite_name) {
  std::cout << "[SUITE] " << suite_name << "\n";
}

inline void print_case_start(const std::string& case_name) {
  std::cout << "  ▸ " << case_name << " ... ";
  std::cout.flush();
}

inline void print_case_success() {
  std::cout << ANSI_GREEN << "ok" << ANSI_RESET << "\n";
}