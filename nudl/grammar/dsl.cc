#include "nudl/grammar/dsl.h"

#include <utility>

#include "absl/strings/match.h"
#include "glog/logging.h"
#include "nudl/grammar/tree_builder.h"

namespace nudl {
namespace grammar {

namespace {
class VectorSaveErrorListener : public antlr4::BaseErrorListener {
 public:
  VectorSaveErrorListener(std::vector<ErrorInfo>* errors,
                          absl::string_view code)
      : antlr4::BaseErrorListener(), errors_(errors), code_(code) {
    CHECK_NOTNULL(errors);
  }
  void syntaxError(antlr4::Recognizer* recognizer,
                   antlr4::Token* offendingSymbol, size_t line,
                   size_t charPositionInLine, const std::string& msg,
                   std::exception_ptr e) override {
    ErrorInfo error;
    error.location.set_line(line);
    error.location.set_column(charPositionInLine);
    error.message = msg;
    error.snippet = TreeUtil::LineSnippet(code_, error.location);
    errors_->emplace_back(std::move(error));
  }

 private:
  std::vector<ErrorInfo>* errors_ = nullptr;
  absl::string_view code_;
};
}  // namespace

absl::Status ParseDataBase::ParseInternal(absl::string_view code_arg,
                                          ParseOptions options) {
  code = std::string(code_arg);
  try {
    input =
        std::make_unique<antlr4::ANTLRInputStream>(code.data(), code.size());
    lexer = std::make_unique<NudlDslLexer>(input.get());
    VectorSaveErrorListener lex_listener(&errors, code);
    lexer->addErrorListener(&lex_listener);
    tokens = std::make_unique<antlr4::CommonTokenStream>(lexer.get());
    tokens->fill();
    if (options.debug_tokens) {
      for (auto token : tokens->getTokens()) {
        std::cout << "   `" << token->toString() << "`" << std::endl;
      }
    }
    parser = std::make_unique<NudlDslParser>(tokens.get());
    if (options.debug_trace) {
      parser->setTrace(true);
    }
    parser->removeErrorListeners();
    VectorSaveErrorListener listener(&errors, code);
    parser->addErrorListener(&listener);
    try {
      tree = CreateTree();
    } catch (const antlr4::RecognitionException& e) {
      errors.emplace_back(ErrorInfo::FromException(e, code));
    }
    lexer->removeErrorListeners();
    parser->removeErrorListeners();
    auto extra_errors = TreeUtil::FindErrors(tree);
    std::move(extra_errors.begin(), extra_errors.end(),
              std::back_inserter(errors));
    if (errors.empty()) {
      absl::string_view visitor_code;
      if (!options.no_intervals) {
        visitor_code = code;
      }
      auto visitor = BuildVisitor(
          parser.get(), code,
          VisitorOptions{options.no_intervals, options.no_interval_positions});
      Visit(visitor.get());
    }
  } catch (const antlr4::ParseCancellationException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 ParseCancel: ", e.what()));
  } catch (const antlr4::CancellationException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 Cancellation: ", e.what()));
  } catch (const antlr4::IllegalStateException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 IllegalState: ", e.what()));
  } catch (const antlr4::IllegalArgumentException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 IllegalArg: ", e.what()));
  } catch (const antlr4::UnsupportedOperationException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 Unsupported: ", e.what()));
  } catch (const antlr4::IOException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 IOError: ", e.what()));
  } catch (const antlr4::RuntimeException& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 RuntimeErr: ", e.what()));
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("ANTLR4 Error: ", e.what()));
  }
  return absl::OkStatus();
}

namespace {
template <class PData, class Proto>
absl::StatusOr<std::unique_ptr<Proto>> ParseProto(
    absl::string_view code, ParseOptions options,
    std::vector<ErrorInfo>* errors) {
  ASSIGN_OR_RETURN(auto data, PData::Parse(code, options));
  if (data->errors.empty()) {
    return {std::move(data->proto)};
  }
  absl::Status result = absl::InvalidArgumentError("Parse errors in code.");
  if (errors) {
    *errors = std::move(data->errors);
  } else {
    result.SetPayload(
        kParseErrorUrl,
        absl::Cord(ErrorInfo::ToParseErrors(data->errors).SerializeAsString()));
  }
  return {std::move(result)};
}
}  // namespace

absl::StatusOr<std::unique_ptr<pb::Module>> ParseModule(
    absl::string_view code, ParseOptions options,
    std::vector<ErrorInfo>* errors) {
  return ParseProto<ModuleParseData, pb::Module>(code, options, errors);
}

absl::StatusOr<std::unique_ptr<pb::TypeSpec>> ParseTypeSpec(
    absl::string_view code, ParseOptions options,
    std::vector<ErrorInfo>* errors) {
  return ParseProto<TypeSpecParseData, pb::TypeSpec>(code, options, errors);
}

}  // namespace grammar

absl::Status& MergeErrorStatus(const absl::Status& src, absl::Status& dest) {
  if (dest.ok()) {
    dest.Update(src);
  } else if (!src.ok()) {
    size_t index = 1;
    src.ForEachPayload(
        [&dest, &index](absl::string_view name, const absl::Cord& payload) {
          if (absl::StartsWith(name, grammar::kParseErrorUrl)) {
            dest.SetPayload(absl::StrCat(name, "/", ++index), payload);
          } else {
            dest.SetPayload(name, payload);
          }
        });
  }
  return dest;
}

}  // namespace nudl
