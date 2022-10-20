#ifndef NUDL_GRAMMAR_TREE_BUILDER_H__
#define NUDL_GRAMMAR_TREE_BUILDER_H__

#include <memory>

#include "NudlDslParserVisitor.h"
#include "absl/strings/string_view.h"

namespace nudl {
namespace grammar {

struct VisitorOptions {
  bool no_intervals = false;
  bool no_interval_positions = false;
};

std::unique_ptr<NudlDslParserVisitor> BuildVisitor(const antlr4::Parser* parser,
                                                   absl::string_view code,
                                                   VisitorOptions options = {});

}  // namespace grammar
}  // namespace nudl

#endif  // NUDL_GRAMMAR_TREE_BUILDER_H__
