#ifndef NUDL_ANALYSIS_BASIC_CONVERTER_H__
#define NUDL_ANALYSIS_BASIC_CONVERTER_H__

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "nudl/analysis/converter.h"
#include "nudl/analysis/function.h"
#include "nudl/analysis/type_spec.h"

namespace nudl {
namespace analysis {

class BasicConvertState : public ConvertState {
 public:
  explicit BasicConvertState(Module* module, size_t indent_delta = 2);
  explicit BasicConvertState(BasicConvertState* superstate);
  ~BasicConvertState() override;

  // The buffer to which we output the code content.
  std::stringstream& out();

  // If this is a sub-state for code generation (that
  // would be appended later to this superstate).
  BasicConvertState* superstate() const;
  // The top of the state tree. e.g. in a function that
  // define another function etc.
  BasicConvertState* top_superstate() const;
  // Current indentation.
  const std::string& indent() const;
  // Advances the indentation.
  void inc_indent();
  // Reduces the indentation.
  void dec_indent();

  // Utility to writer a scoped name to out
  std::stringstream& write_name(const ScopedName& name);
  // Records that this function was processed.
  bool RegisterFunction(Function* fun);

  // Used to mark the current function call in order
  // to use the proper names in identifier & dot expressiosn.
  const Function* in_function_call() const;
  void push_in_function_call(const Function* fun);
  void pop_in_function_call();

  // On stack helper for maintaining the function call stacl.
  class FunctionPusher {
   public:
    FunctionPusher(BasicConvertState* state, const Function* fun)
        : state_(CHECK_NOTNULL(state)) {
      state_->push_in_function_call(CHECK_NOTNULL(fun));
    }
    ~FunctionPusher() { state_->pop_in_function_call(); }

   private:
    BasicConvertState* const state_;
  };

 protected:
  BasicConvertState* const superstate_ = nullptr;
  const size_t indent_delta_;
  std::stringstream out_;
  size_t indent_ = 0;
  std::string indent_str_;
  absl::flat_hash_set<Function*> converted_functions_;
  std::vector<const Function*> in_function_call_;
};

class BasicConverter : public Converter {
 public:
  BasicConverter();

 protected:
  absl::StatusOr<std::unique_ptr<ConvertState>> BeginModule(
      Module* module) const override;
  absl::Status ProcessModule(Module* module,
                             ConvertState* state) const override;
  absl::StatusOr<std::string> FinishModule(
      Module* module, std::unique_ptr<ConvertState> state) const override;

  absl::Status ConvertAssignment(const Assignment& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertEmptyStruct(const EmptyStruct& expression,
                                  ConvertState* state) const override;
  absl::Status ConvertLiteral(const Literal& expression,
                              ConvertState* state) const override;
  absl::Status ConvertIdentifier(const Identifier& expression,
                                 ConvertState* state) const override;
  absl::Status ConvertFunctionResult(const FunctionResultExpression& expression,
                                     ConvertState* state) const override;
  absl::Status ConvertArrayDefinition(
      const ArrayDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertMapDefinition(const MapDefinitionExpression& expression,
                                    ConvertState* state) const override;
  absl::Status ConvertIfExpression(const IfExpression& expression,
                                   ConvertState* state) const override;
  absl::Status ConvertExpressionBlock(const ExpressionBlock& expression,
                                      ConvertState* state) const override;
  absl::Status ConvertIndexExpression(const IndexExpression& expression,
                                      ConvertState* state) const override;
  absl::Status ConvertTupleIndexExpression(
      const TupleIndexExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertLambdaExpression(const LambdaExpression& expression,
                                       ConvertState* state) const override;
  absl::Status ConvertDotAccessExpression(const DotAccessExpression& expression,
                                          ConvertState* state) const override;
  absl::Status ConvertFunctionCallExpression(
      const FunctionCallExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertImportStatement(
      const ImportStatementExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertFunctionDefinition(
      const FunctionDefinitionExpression& expression,
      ConvertState* state) const override;
  absl::Status ConvertSchemaDefinition(
      const SchemaDefinitionExpression& expression,
      ConvertState* state) const override;

  std::string GetTypeString(const TypeSpec* type_spec,
                            BasicConvertState* state) const;
  absl::Status ConvertFunction(Function* fun, ConvertState* state) const;
  absl::Status ConvertBindings(Function* fun, ConvertState* state) const;
};

}  // namespace analysis
}  // namespace nudl

#endif  // NUDL_ANALYSIS_BASIC_CONVERTER_H__
