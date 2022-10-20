#ifndef NUDL_GRAMMAR_DSL_H__
#define NUDL_GRAMMAR_DSL_H__

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "NudlDslLexer.h"
#include "NudlDslParser.h"
#include "NudlDslParserVisitor.h"
#include "absl/status/statusor.h"
#include "antlr4-runtime.h"  // NOLINT
#include "nudl/grammar/tree_util.h"
#include "nudl/status/status.h"

namespace nudl {
namespace grammar {

struct ParseOptions {
  // Include code intervals and code in parsed proto structure.
  bool no_intervals = false;
  // Clear the positions in the code intervals.
  bool no_interval_positions = false;
  // Logs the lexer tokens for each parse.
  bool debug_tokens = false;
  // Trace the antlr parse process.
  bool debug_trace = false;
};

struct ParseDataBase {
  // The code we have to parse. We need a copy for stable refernces
  // of code locations.
  std::string code;

  // The code as an antlr input stream.
  std::unique_ptr<antlr4::ANTLRInputStream> input;
  // Splits the input string in tokens.
  std::unique_ptr<NudlDslLexer> lexer;
  // The lexer tokens in the input stream.
  std::unique_ptr<antlr4::CommonTokenStream> tokens;
  // Parses the input stream into a parse tree.
  std::unique_ptr<NudlDslParser> parser;
  // The parsed tree - owned by the parser, who deallocates it.
  antlr4::tree::ParseTree* tree = nullptr;
  // Errors found while parsing (if any).
  std::vector<ErrorInfo> errors;

  virtual ~ParseDataBase() {}

 protected:
  absl::Status ParseInternal(absl::string_view code_arg, ParseOptions options);

  virtual antlr4::tree::ParseTree* CreateTree() = 0;
  virtual void Visit(NudlDslParserVisitor* visitor) = 0;
};

template <class Proto, antlr4::tree::ParseTree* (*TreeSelector)(NudlDslParser*)>
struct ConfigurableParseData : public ParseDataBase {
  // The parsed module proto.
  std::unique_ptr<Proto> proto;

  static absl::StatusOr<
      std::unique_ptr<ConfigurableParseData<Proto, TreeSelector>>>
  Parse(absl::string_view code_arg, ParseOptions options = {}) {
    auto data = std::make_unique<ConfigurableParseData<Proto, TreeSelector>>();
    RETURN_IF_ERROR(data->ParseInternal(code_arg, options));
    return {std::move(data)};
  }

 protected:
  antlr4::tree::ParseTree* CreateTree() override {
    return TreeSelector(parser.get());
  }
  void Visit(NudlDslParserVisitor* visitor) override {
    proto = std::make_unique<Proto>(std::any_cast<Proto>(visitor->visit(tree)));
  }
};

inline antlr4::tree::ParseTree* ExtractModule(NudlDslParser* parser) {
  return parser->module();
}
using ModuleParseData = ConfigurableParseData<pb::Module, ExtractModule>;

inline antlr4::tree::ParseTree* ExtractTypeExpression(NudlDslParser* parser) {
  return parser->typeExpression();
}
using TypeSpecParseData =
    ConfigurableParseData<pb::TypeSpec, ExtractTypeExpression>;

inline constexpr absl::string_view kParseErrorUrl = "nudl.nuna.com/ParseError";
inline constexpr absl::string_view kParseCodeUrl = "nudl.nuna.com/ParseCode";
inline constexpr absl::string_view kParseFileUrl = "nudl.nuna.com/ParseFile";

// Parses a module from provided string.
// In case of errors they are attached to the status as payload, under
// kModuleParseErrorUrl, or returned in errors argument, if provided.
absl::StatusOr<std::unique_ptr<pb::Module>> ParseModule(
    absl::string_view code, ParseOptions options = {},
    std::vector<ErrorInfo>* errors = nullptr);

absl::StatusOr<std::unique_ptr<pb::TypeSpec>> ParseTypeSpec(
    absl::string_view code, ParseOptions options = {},
    std::vector<ErrorInfo>* errors = nullptr);

template <class Pb>
std::string ToDsl(const Pb& proto) {
  return proto.ShortDebugString();  // dummy implementation - to be removed
}

// TODO(catalin): need explicit specializations to actually generate dsl. E.g.
// template<> std::string ToDsl<pb::Literal>(const pb::Literal& proto);
// etc. Then remove the dummy implementation above.

}  // namespace grammar

struct ParseFileInfo {
  std::string filename{};
};

struct ParseFileContent {
  std::string code{};
};

// Merges the error payloads from src into dest.
absl::Status& MergeErrorStatus(const absl::Status& src, absl::Status& dest);

namespace status {

template <>
inline StatusWriter& StatusWriter::operator<<(
    const nudl::grammar::ErrorInfo& err) {
  ++payload_id_;
  nudl::grammar::ErrorInfo composed_err(err);
  composed_err.message = absl::StrCat(err.message, ": ", status_.message());
  if (!message_.empty()) {
    absl::StrAppend(&composed_err.message,
                    status_.message().empty() ? "" : "; ", message_);
  }
  status_.SetPayload(
      absl::StrCat(nudl::grammar::kParseErrorUrl, "/", payload_id_),
      absl::Cord(composed_err.ToProto().SerializeAsString()));
  return *this;
}
template <>
inline StatusWriter& StatusWriter::operator<<(
    const nudl::pb::ErrorInfo& err) {
  ++payload_id_;
  nudl::pb::ErrorInfo composed_err(err);
  std::string message =
      absl::StrCat(err.error_message(), ": ", status_.message());
  if (!message_.empty()) {
    absl::StrAppend(&message, status_.message().empty() ? "" : "; ", message_);
  }
  composed_err.set_error_message(message);
  status_.SetPayload(
      absl::StrCat(nudl::grammar::kParseErrorUrl, "/", payload_id_),
      absl::Cord(composed_err.SerializeAsString()));
  return *this;
}
template <>
inline StatusWriter& StatusWriter::operator<<(
    const nudl::ParseFileInfo& info) {
  status_.SetPayload(
      absl::StrCat(nudl::grammar::kParseFileUrl, "/", payload_id_),
      absl::Cord(info.filename));
  return *this;
}
template <>
inline StatusWriter& StatusWriter::operator<<(
    const nudl::ParseFileContent& info) {
  status_.SetPayload(
      absl::StrCat(nudl::grammar::kParseCodeUrl, "/", payload_id_),
      absl::Cord(info.code));
  return *this;
}

}  // namespace status
}  // namespace nudl

#endif  // NUDL_GRAMMAR_DSL_H__
