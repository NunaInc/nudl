//
// Copyright 2022 Nuna inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Note: Original code adapted from:
// https://github.com/google/nucleus/blob/master
//  .. /nucleus/testing/protocol-buffer-matchers.h
//
#ifndef NUDL_CONVERSION_CONVERTER_H__
#define NUDL_CONVERSION_CONVERTER_H__

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nudl/analysis/analysis.h"

namespace nudl {
namespace conversion {

class ConvertState {
 public:
  explicit ConvertState(analysis::Module* module);
  virtual ~ConvertState();

  analysis::Module* module() const;

 protected:
  analysis::Module* module_;
};

class Converter {
 public:
  Converter();
  virtual ~Converter();

  absl::StatusOr<std::string> ConvertModule(analysis::Module* module) const;

 protected:
  virtual absl::StatusOr<std::unique_ptr<ConvertState>> BeginModule(
      analysis::Module* module) const = 0;
  virtual absl::Status ProcessModule(analysis::Module* module,
                                     ConvertState* state) const = 0;
  virtual absl::StatusOr<std::string> FinishModule(
      analysis::Module* module, std::unique_ptr<ConvertState> state) const = 0;

  virtual absl::Status ConvertExpression(const analysis::Expression& expression,
                                         ConvertState* state) const;

  // Individual expression conversion:
  virtual absl::Status ConvertAssignment(const analysis::Assignment& expression,
                                         ConvertState* state) const = 0;
  virtual absl::Status ConvertEmptyStruct(
      const analysis::EmptyStruct& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertLiteral(const analysis::Literal& expression,
                                      ConvertState* state) const = 0;
  virtual absl::Status ConvertIdentifier(const analysis::Identifier& expression,
                                         ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionResult(
      const analysis::FunctionResultExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertArrayDefinition(
      const analysis::ArrayDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertMapDefinition(
      const analysis::MapDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertIfExpression(
      const analysis::IfExpression& expression, ConvertState* state) const = 0;
  virtual absl::Status ConvertExpressionBlock(
      const analysis::ExpressionBlock& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertIndexExpression(
      const analysis::IndexExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertTupleIndexExpression(
      const analysis::TupleIndexExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertLambdaExpression(
      const analysis::LambdaExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertDotAccessExpression(
      const analysis::DotAccessExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionCallExpression(
      const analysis::FunctionCallExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertImportStatement(
      const analysis::ImportStatementExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertFunctionDefinition(
      const analysis::FunctionDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertSchemaDefinition(
      const analysis::SchemaDefinitionExpression& expression,
      ConvertState* state) const = 0;
  virtual absl::Status ConvertTypeDefinition(
      const analysis::TypeDefinitionExpression& expression,
      ConvertState* state) const = 0;
};

}  // namespace conversion
}  // namespace nudl

#endif  // NUDL_CONVERSION_CONVERTER_H__
