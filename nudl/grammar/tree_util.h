#ifndef NUDL_GRAMMAR_TREE_UTIL_H__
#define NUDL_GRAMMAR_TREE_UTIL_H__

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "antlr4-runtime.h"  // NOLINT
#include "nudl/proto/dsl.pb.h"

namespace nudl {
namespace grammar {

// Represen
struct TokenNode {
  using Tokens = std::vector<TokenNode>;

  // We set one and only one of these members. Cannot use something
  // like absl::variant as that would require recursion. We can use this
  // here however, as of C++17 can define vectors of incomplete types.
  absl::optional<std::string> text;
  absl::optional<Tokens> tokens;

  TokenNode() : tokens(Tokens()) {}
  explicit TokenNode(std::string&& text) : text(std::move(text)) {}
  explicit TokenNode(Tokens&& tokens) : tokens(std::move(tokens)) {}
  explicit TokenNode(const antlr4::Token& token);
  bool has_text() const { return text.has_value(); }
  bool has_tokens() const { return tokens.has_value(); }
  struct RecomposedStruct {
    std::string text;
    std::string first_token;
    std::string last_token;
    RecomposedStruct() {}
    RecomposedStruct(absl::string_view text, absl::string_view first_token,
                     absl::string_view last_token)
        : text(text), first_token(text), last_token(text) {}
  };
  // A one-line tree representation of this node.
  std::string ToString() const;
  // Recomposes the string in a token tree, returning also the
  // first and last token.
  RecomposedStruct Recompose() const;
};

struct ErrorInfo {
  pb::CodeLocation location;
  std::string message;
  std::string snippet;
  std::string ToString() const;
  pb::ErrorInfo ToProto() const;
  std::string ToCompileErrorString(absl::string_view filename) const;
  static pb::ParseErrors ToParseErrors(
      const absl::Span<const ErrorInfo>& errors);

  static ErrorInfo FromParseTree(antlr4::tree::ParseTree* pt,
                                 absl::string_view code);
  static ErrorInfo FromToken(const antlr4::Token& token,
                             absl::string_view code);
  static ErrorInfo FromException(const antlr4::RecognitionException& e,
                                 absl::string_view code);
  static ErrorInfo FromProto(const pb::ErrorInfo& info);
  static std::vector<ErrorInfo> FromParseErrors(const pb::ParseErrors& errors);
};

class TreeUtil {
 public:
  // Returns true of this node has children on a second level below.
  static bool HasGrandchildren(const antlr4::tree::ParseTree& pt);

  // Returns a sort string describing the type of the tree.
  static std::string TreeTypeString(const antlr4::tree::ParseTree& pt,
                                    const antlr4::Parser& parser);

  // Returns the tree as a nicely formatted multiline string.
  // We have to use the non const ref due to underlying API requirement
  // from antlr.
  static std::string ToString(antlr4::tree::ParseTree* pt,
                              const antlr4::Parser& parser);

  // Returns the tree as a compact, one-line string.
  static std::string ToShortString(antlr4::tree::ParseTree* pt,
                                   const antlr4::Parser& parser);

  // Returns the begin location (line&column) for the provided parse tree.
  static pb::CodeLocation GetBeginLocation(const antlr4::tree::ParseTree& pt);

  // Returns the end position (line&column) for the provided parse tree.
  static pb::CodeLocation GetEndLocation(const antlr4::tree::ParseTree& pt);

  // Returns the interval [begin, end) location for the provided parse tree.
  static pb::CodeInterval GetInterval(const antlr4::tree::ParseTree& pt);

  // Returns the line from provided code location to end-of-line.
  static absl::string_view LineSnippet(absl::string_view code,
                                       const pb::CodeLocation& location);

  // Returns the string between two code locations.
  static absl::string_view CodeSnippet(absl::string_view code,
                                       const pb::CodeLocation& begin,
                                       const pb::CodeLocation& end);

  // Converts the provided tree to a TokenNode structure, keeping the
  // Tokens as strings and Rule nodes as lists.
  static TokenNode TokenNodeFromTree(const antlr4::tree::ParseTree* pt);

  // Extracts the corresponding string from a tree node.
  static std::string Recompose(const antlr4::tree::ParseTree* pt);

  // Extracts errors from a tree.
  static std::vector<ErrorInfo> FindErrors(antlr4::tree::ParseTree* pt,
                                           absl::string_view code = "");
};

}  // namespace grammar
}  // namespace nudl

#endif  // NUDL_GRAMMAR_TREE_UTIL_H__
