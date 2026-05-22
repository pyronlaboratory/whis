#pragma once

#include <string>
#include <vector>

namespace whis::ast {
struct Node {
  std::string raw_content;
};
struct Program {
  std::vector<Node> statements;
};
}  // namespace whis::ast
