#include "nudl/grammar/tree_util.h"

#include <algorithm>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"

namespace nudl {
namespace grammar {

bool TreeUtil::HasGrandchildren(const antlr4::tree::ParseTree& pt) {
  // Returns true of this node has children on a second level below.
  for (const antlr4::tree::ParseTree* child : pt.children) {
    if (!child->children.empty()) {
      return true;
    }
  }
  return false;
}

// If pt is a terminal node, return the corresponding token, else null.
const antlr4::Token* GetToken(const antlr4::tree::ParseTree& pt) {
  if (antlr4::tree::TerminalNode::is(&pt)) {
    return static_cast<const antlr4::tree::TerminalNode&>(pt).getSymbol();
  }
  return nullptr;
}

std::string TreeUtil::TreeTypeString(const antlr4::tree::ParseTree& pt,
                                     const antlr4::Parser& parser) {
  switch (pt.getTreeType()) {
    case antlr4::tree::ParseTreeType::TERMINAL: {
      auto token = GetToken(pt);
      if (!token) {
        return "T";
      }
      return absl::StrCat("TOK: `", token->toString(), "`");
    }
    case antlr4::tree::ParseTreeType::ERROR:
      return "Error";
    case antlr4::tree::ParseTreeType::RULE:
      return "R";
    default:
      return "X";
  }
}

std::string ToStringImpl(antlr4::tree::ParseTree* pt,
                         const antlr4::Parser& parser,
                         const std::string& indent,
                         const std::string& first_indent) {
  if (!pt) {
    return "";
  }
  std::string s =
      absl::StrCat(TreeUtil::TreeTypeString(*pt, parser), " ",
                   antlr4::tree::Trees::getNodeText(pt, parser.getRuleNames()));
  if (pt->children.empty()) {
    return s;
  }
  const std::string sub_indent(absl::StrCat(indent, "  |   "));
  const std::string sub_first_indent(absl::StrCat(indent, "  +-- "));
  std::string buf;
  if (first_indent.empty()) {
    absl::StrAppend(&buf, indent, "+-- ", s, "\n");
  } else {
    absl::StrAppend(&buf, first_indent, s, "\n");
  }
  if (!TreeUtil::HasGrandchildren(*pt)) {
    absl::StrAppend(&buf, indent, "    +-- ");
    for (auto child : pt->children) {
      absl::StrAppend(&buf, ToStringImpl(child, parser, sub_indent, ""));
    }
    absl::StrAppend(&buf, "\n");
    return buf;
  }
  for (size_t i = 0; i < pt->children.size(); ++i) {
    auto child = pt->children[i];
    if (!child) {
      continue;
    }
    if (child->children.empty()) {
      absl::StrAppend(&buf, indent, "  +-- ");
    }
    const std::string crt_indent(i + 1 < pt->children.size()
                                     ? sub_indent
                                     : absl::StrCat(indent, "    "));
    absl::StrAppend(&buf,
                    ToStringImpl(child, parser, crt_indent, sub_first_indent));
    if (child->children.empty()) {
      absl::StrAppend(&buf, "\n");
    }
  }
  return buf;
}

std::string TreeUtil::ToString(antlr4::tree::ParseTree* pt,
                               const antlr4::Parser& parser) {
  return ToStringImpl(pt, parser, "", "");
}

static constexpr absl::string_view kIndent = "  ";

std::string ProcessShortStringChildren(const std::string& node,
                                       const std::vector<std::string>& children,
                                       absl::string_view indent,
                                       size_t max_len) {
  std::string child_indent(absl::StrCat(indent, kIndent));
  std::vector<std::string> lines;
  lines.emplace_back(absl::StrCat(node, "("));
  bool is_first = true;
  for (auto& child : children) {
    if (child.find('\n') != std::string::npos) {
      for (auto line :
           absl::StrSplit(absl::StrCat(child_indent, child), "\n")) {
        lines.emplace_back(std::string(line));
      }
    } else if (lines.back().size() + child.size() < max_len) {
      absl::StrAppend(&lines.back(), is_first ? "" : " ", child);
    } else {
      lines.emplace_back(absl::StrCat(child_indent, child));
    }
    is_first = false;
  }
  if (lines.size() == 1) {
    absl::StrAppend(&lines.back(), ")");
  } else {
    lines.emplace_back(absl::StrCat(indent, ")"));
  }
  return absl::StrJoin(lines, "\n");
}

std::string ToShortStringImpl(antlr4::tree::ParseTree* pt,
                              const antlr4::Parser& parser,
                              absl::string_view indent, size_t max_len) {
  if (!pt) {
    return "";
  }
  auto token = GetToken(*pt);
  if (token) {
    if (token->getText() == "<EOF>") {
      return "";
    }
    return token->getText();
  }
  if (pt->children.size() == 1) {
    return ToShortStringImpl(pt->children.front(), parser, indent, max_len);
  }
  std::string s(antlr4::tree::Trees::getNodeText(pt, parser.getRuleNames()));
  if (pt->children.empty()) {
    return s;
  }
  std::vector<std::string> children;
  std::string child_indent(absl::StrCat(indent, kIndent));
  bool is_split = false;
  size_t child_size = 0;
  std::string result;
  for (auto child : pt->children) {
    if (!child) {
      continue;
    }
    std::string crt = ToShortStringImpl(child, parser, child_indent, max_len);
    if (crt.empty()) {
      continue;
    }
    is_split |= crt.find('\n') != std::string::npos;
    child_size += crt.size();
    children.emplace_back(std::move(crt));
  }
  if (children.empty()) {
    return s;
  }
  if (!is_split &&
      s.size() + (children.size() - 1) + child_size + indent.size() + 2 <=
          max_len) {
    return absl::StrCat(s, "(", absl::StrJoin(children, " "), ")");
  }
  return ProcessShortStringChildren(s, children, indent, max_len);
}

std::string TreeUtil::ToShortString(antlr4::tree::ParseTree* pt,
                                    const antlr4::Parser& parser) {
  return ToShortStringImpl(pt, parser, "", 75);
}

pb::CodeLocation GetTokenBeginLocation(const antlr4::Token& token) {
  pb::CodeLocation location;
  location.set_position(token.getStartIndex());
  location.set_line(token.getLine());
  location.set_column(token.getCharPositionInLine());
  return location;
}

pb::CodeLocation GetTokenEndLocation(const antlr4::Token& token) {
  pb::CodeLocation location;
  location.set_position(token.getStopIndex() + 1);
  location.set_line(token.getLine());
  location.set_column(token.getCharPositionInLine() + token.getText().size());
  return location;
}

pb::CodeLocation TreeUtil::GetBeginLocation(const antlr4::tree::ParseTree& pt) {
  if (!pt.children.empty()) {
    return GetBeginLocation(*pt.children.front());
  }
  pb::CodeLocation location;
  auto token = GetToken(pt);
  if (token) {
    location = GetTokenBeginLocation(*token);
  }
  return location;
}

pb::CodeLocation TreeUtil::GetEndLocation(const antlr4::tree::ParseTree& pt) {
  if (!pt.children.empty()) {
    return GetEndLocation(*pt.children.back());
  }
  pb::CodeLocation location;
  auto token = GetToken(pt);
  if (token) {
    location = GetTokenEndLocation(*token);
  }
  return location;
}

pb::CodeInterval TreeUtil::GetInterval(const antlr4::tree::ParseTree& pt) {
  pb::CodeInterval interval;
  *interval.mutable_begin() = GetBeginLocation(pt);
  *interval.mutable_end() = GetEndLocation(pt);
  return interval;
}

absl::string_view TreeUtil::LineSnippet(absl::string_view code,
                                        const pb::CodeLocation& location) {
  if (code.empty()) {
    return "";
  }
  if (location.has_position()) {
    size_t size = 0;
    for (size_t index = location.position();
         index < code.size() && code[index] != '\n'; ++index) {
      ++size;
    }
    if (!size) {
      return "";
    }
    return code.substr(location.position(), size);
  }
  if (!location.has_line() || !location.has_column()) {
    return "";
  }
  size_t lineno = 1;
  for (absl::string_view line : absl::StrSplit(code, '\n')) {
    if (lineno == location.line()) {
      if (location.column() < line.size()) {
        return line.substr(location.column());
      } else {
        return "";
      }
    }
    ++lineno;
  }
  return "";
}

static absl::string_view StringBetween(absl::string_view code, size_t begin,
                                       size_t end) {
  if (begin > code.size() || begin >= end) {
    return "";
  }
  const size_t delta = end - begin;
  if (begin + delta >= code.size()) {
    return code.substr(begin);
  }
  return code.substr(begin, delta);
}

absl::string_view TreeUtil::CodeSnippet(absl::string_view code,
                                        const pb::CodeLocation& begin,
                                        const pb::CodeLocation& end) {
  if (begin.has_position() && end.has_position()) {
    return StringBetween(code, begin.position(), end.position());
  }
  if (!begin.has_line() || !begin.has_column() || !end.has_line() ||
      !end.has_column() || begin.line() > end.line() ||
      (begin.line() == end.line() && begin.column() >= end.column())) {
    return "";
  }
  size_t lineno = 1;
  size_t crtpos = 0;
  size_t startpos = 0;
  for (absl::string_view line : absl::StrSplit(code, '\n')) {
    if (lineno == begin.line()) {
      if (lineno == end.line()) {
        // Special case returning from same line.
        return StringBetween(line, begin.column(), end.column());
      }
      startpos =
          crtpos + std::min(static_cast<size_t>(begin.column()), line.size());
    } else if (lineno == end.line()) {
      const size_t endpos =
          crtpos + std::min(static_cast<size_t>(end.column()), line.size());
      return StringBetween(code, startpos, endpos);
    }
    ++lineno;
    crtpos += line.size() + 1;
  }
  return StringBetween(code, startpos, code.size());
}

// Helper to get the text of a antlr token w/o the <EOF>.
TokenNode::TokenNode(const antlr4::Token& token) : text(token.getText()) {}

bool IsAlpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}
bool IsAlphaNum(char c) { return ('0' <= c && c <= '0') || IsAlpha(c); }

bool IsIdentifier(absl::string_view kw) {
  bool is_first = true;
  for (char c : kw) {
    if (is_first) {
      is_first = false;
      if (!IsAlpha(c)) {
        return false;
      }
    } else if (!IsAlphaNum(c)) {
      return false;
    }
  }
  return true;
}

bool RecomposeWithSpace(absl::string_view last, absl::string_view crt) {
  static const auto kNoSpaceContinuation =
      new absl::flat_hash_set<absl::string_view>({".", "(", "["});
  static const auto kSeparators = new absl::flat_hash_set<absl::string_view>(
      {".", "(", "[", "]", ")", ",", ";"});
  static const auto kSpacedKw = new absl::flat_hash_set<absl::string_view>(
      {"in", "not", "and", "or", "xor", "between"});
  if (last.empty() || kNoSpaceContinuation->contains(last)) {
    return false;
  }
  if ((last == ">" && crt == ">") || (last == "<" && crt == "<<")) {
    return false;
  }
  return (!kSeparators->contains(crt) ||
          (crt == "(" && (!IsIdentifier(last) || kSpacedKw->contains(last))));
}

std::string TokenNode::ToString() const {
  if (has_text()) {
    return absl::StrCat("`", text.value(), "`");
  }
  std::string result("[ ");
  for (const auto& token : tokens.value()) {
    absl::StrAppend(&result, token.ToString(), " ");
  }
  absl::StrAppend(&result, "]");
  return result;
}

TokenNode::RecomposedStruct TokenNode::Recompose() const {
  if (has_text()) {
    return TokenNode::RecomposedStruct(text.value(), text.value(),
                                       text.value());
  }
  TokenNode::RecomposedStruct result;
  bool first_token = true;
  for (const auto& token : tokens.value()) {
    auto crt = token.Recompose();
    if (first_token) {
      result.first_token = crt.first_token;
      first_token = false;
    } else if (RecomposeWithSpace(result.last_token, crt.first_token)) {
      absl::StrAppend(&result.text, " ");
    }
    absl::StrAppend(&result.text, crt.text);
    result.last_token = std::move(crt.last_token);
  }
  return result;
}

TokenNode TreeUtil::TokenNodeFromTree(const antlr4::tree::ParseTree* pt) {
  if (!pt) {
    return TokenNode();
  }
  auto token = GetToken(*pt);
  if (token) {
    return TokenNode(*token);
  }
  TokenNode::Tokens tokens;
  tokens.reserve(pt->children.size());
  for (auto child : pt->children) {
    tokens.emplace_back(TokenNodeFromTree(child));
  }
  return TokenNode(std::move(tokens));
}

std::vector<ErrorInfo> TreeUtil::FindErrors(antlr4::tree::ParseTree* pt,
                                            absl::string_view code) {
  std::vector<ErrorInfo> result;
  if (!pt) {
    return result;
  }
  if (antlr4::tree::ErrorNode::is(pt)) {
    ErrorInfo error;
    error.location = GetBeginLocation(*pt);
    error.message = pt->toString();
    error.snippet = LineSnippet(code, error.location);
    result.emplace_back(std::move(error));
  } else {
    for (auto child : pt->children) {
      auto crt = FindErrors(child, code);
      std::move(crt.begin(), crt.end(), std::back_inserter(result));
    }
  }
  return result;
}

std::string TreeUtil::Recompose(const antlr4::tree::ParseTree* pt) {
  return TokenNodeFromTree(pt).Recompose().text;
}

ErrorInfo ErrorInfo::FromParseTree(antlr4::tree::ParseTree* pt,
                                   absl::string_view code) {
  CHECK_NOTNULL(pt);
  ErrorInfo error;
  error.location = TreeUtil::GetBeginLocation(*pt);
  if (antlr4::tree::ErrorNode::is(pt)) {
    error.message = pt->toString();
  } else {
    error.message = "Parsing error encountered";
  }
  error.snippet = TreeUtil::LineSnippet(code, error.location);
  return error;
}

ErrorInfo ErrorInfo::FromToken(const antlr4::Token& token,
                               absl::string_view code) {
  ErrorInfo error;
  error.location = GetTokenBeginLocation(token);
  error.message = absl::StrCat("Parsing error encountered on token `",
                               token.getText(), "`");
  error.snippet = TreeUtil::LineSnippet(code, error.location);
  return error;
}

ErrorInfo ErrorInfo::FromException(const antlr4::RecognitionException& e,
                                   absl::string_view code) {
  ErrorInfo error;
  if (e.getOffendingToken()) {
    error = FromToken(*e.getOffendingToken(), code);
  } else if (e.getCtx()) {
    error = FromParseTree(e.getCtx(), code);
  } else {
    error.message = absl::StrCat("Parsing exception encountered");
  }
  absl::StrAppend(&error.message, " - ", e.what());
  return error;
}

pb::ErrorInfo ErrorInfo::ToProto() const {
  pb::ErrorInfo error;
  *error.mutable_location() = location;
  error.set_error_message(message);
  error.set_snippet(snippet);
  return error;
}

pb::ParseErrors ErrorInfo::ToParseErrors(
    const absl::Span<const ErrorInfo>& errors) {
  pb::ParseErrors pb_errors;
  for (auto error : errors) {
    *pb_errors.add_error() = error.ToProto();
  }
  return pb_errors;
}

std::vector<ErrorInfo> ErrorInfo::FromParseErrors(
    const pb::ParseErrors& errors) {
  std::vector<ErrorInfo> infos;
  infos.reserve(errors.error().size());
  for (const auto& error : errors.error()) {
    infos.emplace_back(ErrorInfo::FromProto(error));
  }
  return infos;
}

ErrorInfo ErrorInfo::FromProto(const pb::ErrorInfo& info) {
  return ErrorInfo{info.location(), info.error_message(), info.snippet()};
}

std::string ErrorInfo::ToString() const {
  return absl::StrCat("line ", location.line(), ":", location.column(), " ",
                      message, ": at `", snippet, "`");
}

std::string ErrorInfo::ToCompileErrorString(absl::string_view filename) const {
  return absl::StrCat(filename, ":", location.line(), ":", location.column(),
                      ": error: ", message, "\n`", snippet, "`");
}

}  // namespace grammar
}  // namespace nudl
