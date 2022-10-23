#include "nudl/analysis/names.h"

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nudl/status/testing.h"
#include "nudl/testing/protobuf_matchers.h"

namespace nudl {
namespace analysis {

TEST(NameUtil, Validations) {
  EXPECT_TRUE(NameUtil::IsValidName("a"));
  EXPECT_TRUE(NameUtil::IsValidName("_"));
  EXPECT_TRUE(NameUtil::IsValidName("aXc1_z"));
  EXPECT_TRUE(NameUtil::IsValidName("AB_cde0_12"));
  EXPECT_FALSE(NameUtil::IsValidName(""));
  EXPECT_FALSE(NameUtil::IsValidName("A$"));
  EXPECT_FALSE(NameUtil::IsValidName("0"));
  EXPECT_FALSE(NameUtil::IsValidName("$"));
  EXPECT_FALSE(NameUtil::IsValidName("AbC#x"));
  EXPECT_FALSE(NameUtil::IsValidName("AbC&x"));

  ASSERT_OK_AND_ASSIGN(auto name, NameUtil::ValidatedName("ab12"));
  EXPECT_EQ(name, "ab12");
  EXPECT_RAISES(NameUtil::ValidatedName("0").status(), InvalidArgument);

  EXPECT_TRUE(NameUtil::IsValidModuleName("a"));
  EXPECT_TRUE(NameUtil::IsValidModuleName("a.b"));
  EXPECT_TRUE(NameUtil::IsValidModuleName("a._.b"));
  EXPECT_TRUE(NameUtil::IsValidModuleName(""));
  EXPECT_FALSE(NameUtil::IsValidModuleName("a.a$.b"));
  EXPECT_FALSE(NameUtil::IsValidModuleName("a..b"));
  EXPECT_FALSE(NameUtil::IsValidModuleName("a.b."));
  EXPECT_FALSE(NameUtil::IsValidModuleName(".a.b"));

  ASSERT_OK_AND_ASSIGN(name, NameUtil::ValidatedModuleName("a.b.c"));
  EXPECT_EQ(name, "a.b.c");
  EXPECT_RAISES(NameUtil::ValidatedModuleName("a..c").status(),
                InvalidArgument);
}

TEST(NameUtil, FromIdentifiers) {
  pb::Identifier identifier;
  EXPECT_RAISES(NameUtil::GetModuleName(identifier).status(), InvalidArgument);
  EXPECT_RAISES(NameUtil::GetObjectName(identifier).status(), InvalidArgument);
  identifier.add_name("foo");
  ASSERT_OK_AND_ASSIGN(auto name, NameUtil::GetModuleName(identifier));
  EXPECT_EQ(name, "");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetObjectName(identifier));
  EXPECT_EQ(name, "foo");
  identifier.add_name("bar");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetModuleName(identifier));
  EXPECT_EQ(name, "foo");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetObjectName(identifier));
  EXPECT_EQ(name, "bar");
  identifier.add_name("baz");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetModuleName(identifier));
  EXPECT_EQ(name, "foo.bar");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetObjectName(identifier));
  EXPECT_EQ(name, "baz");
  identifier.add_name("q#x");
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetModuleName(identifier));
  EXPECT_EQ(name, "foo.bar.baz");
  EXPECT_RAISES(NameUtil::GetObjectName(identifier).status(), InvalidArgument);
  identifier.add_name("qux");
  EXPECT_RAISES(NameUtil::GetModuleName(identifier).status(), InvalidArgument);
  ASSERT_OK_AND_ASSIGN(name, NameUtil::GetObjectName(identifier));
  EXPECT_EQ(name, "qux");
}

TEST(ScopeName, ParseFull) {
  ASSERT_OK_AND_ASSIGN(ScopeName name, ScopeName::Parse("foo.bar::baz::qux"));
  EXPECT_EQ(name.size(), 4);
  EXPECT_FALSE(name.empty());
  EXPECT_EQ(name.name(), "foo.bar::baz::qux");
  EXPECT_EQ(name.module_name(), "foo.bar");
  EXPECT_EQ(name.function_name(), "baz::qux");
  EXPECT_EQ(name.hash(), std::hash<std::string>()(name.name()));
  EXPECT_EQ(name.PrefixName(0), "");
  EXPECT_EQ(name.PrefixName(1), "foo");
  EXPECT_EQ(name.PrefixName(2), "foo.bar");
  EXPECT_EQ(name.PrefixName(3), "foo.bar::baz");
  EXPECT_EQ(name.PrefixName(4), name.name());
  EXPECT_EQ(name.PrefixName(5), name.name());
  EXPECT_EQ(name.PrefixScopeName(1).module_name(), "foo");
  EXPECT_EQ(name.PrefixScopeName(1).function_name(), "");
  EXPECT_EQ(name.PrefixScopeName(2).module_name(), "foo.bar");
  EXPECT_EQ(name.PrefixScopeName(2).function_name(), "");
  EXPECT_EQ(name.PrefixScopeName(3).module_name(), "foo.bar");
  EXPECT_EQ(name.PrefixScopeName(3).function_name(), "baz");
  EXPECT_EQ(name.PrefixScopeName(4).module_name(), name.module_name());
  EXPECT_EQ(name.PrefixScopeName(4).function_name(), name.function_name());

  EXPECT_EQ(name.SuffixName(5), "");
  EXPECT_EQ(name.SuffixScopeName(5).name(), "");
  EXPECT_EQ(name.SuffixName(4), "");
  EXPECT_EQ(name.SuffixScopeName(4).name(), "");
  EXPECT_EQ(name.SuffixName(3), "::qux");
  EXPECT_EQ(name.SuffixScopeName(3).module_name(), "");
  EXPECT_EQ(name.SuffixScopeName(3).function_name(), "qux");
  EXPECT_EQ(name.SuffixName(2), "::baz::qux");
  EXPECT_EQ(name.SuffixScopeName(2).module_name(), "");
  EXPECT_EQ(name.SuffixScopeName(2).function_name(), "baz::qux");
  EXPECT_EQ(name.SuffixName(1), "bar::baz::qux");
  EXPECT_EQ(name.SuffixScopeName(1).module_name(), "bar");
  EXPECT_EQ(name.SuffixScopeName(1).function_name(), "baz::qux");
  EXPECT_EQ(name.SuffixName(0), "foo.bar::baz::qux");
  EXPECT_EQ(name.SuffixScopeName(0).module_name(), "foo.bar");
  EXPECT_EQ(name.SuffixScopeName(0).function_name(), "baz::qux");
  ASSERT_OK_AND_ASSIGN(auto subname, name.Submodule("extra"));
  EXPECT_EQ(subname.name(), "foo.bar.extra::baz::qux");
  ASSERT_OK_AND_ASSIGN(subname, name.Subfunction("extra"));
  EXPECT_EQ(subname.name(), "foo.bar::baz::qux::extra");
  EXPECT_RAISES(name.Submodule("1extra").status(), InvalidArgument);
  EXPECT_RAISES(name.Subfunction("1extra").status(), InvalidArgument);
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), name.function_names()),
            name.name());
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), {}), name.module_name());
  EXPECT_EQ(ScopeName::Recompose({}, name.function_names()),
            absl::StrCat("::", name.function_name()));

  ASSERT_OK_AND_ASSIGN(ScopeName name2, ScopeName::Parse("::foo::bar"));
  EXPECT_EQ(name2.name(), "::foo::bar");
  EXPECT_EQ(name2.PrefixName(0), "");
  EXPECT_EQ(name2.PrefixName(1), "::foo");
  EXPECT_EQ(name2.SuffixName(2), "");
  EXPECT_EQ(name2.SuffixName(1), "::bar");
  EXPECT_EQ(name2.SuffixName(0), "::foo::bar");
}

TEST(ScopeName, ParseModule) {
  ASSERT_OK_AND_ASSIGN(ScopeName name, ScopeName::Parse("foo.bar.baz"));
  EXPECT_EQ(name.size(), 3);
  EXPECT_FALSE(name.empty());
  EXPECT_EQ(name.name(), "foo.bar.baz");
  EXPECT_EQ(name.module_name(), "foo.bar.baz");
  EXPECT_EQ(name.function_name(), "");
  EXPECT_EQ(name.hash(), std::hash<std::string>()(name.name()));
  EXPECT_EQ(name.PrefixName(0), "");
  EXPECT_EQ(name.PrefixName(1), "foo");
  EXPECT_EQ(name.PrefixName(2), "foo.bar");
  EXPECT_EQ(name.PrefixName(3), name.name());
  EXPECT_EQ(name.PrefixName(4), name.name());
  ASSERT_OK_AND_ASSIGN(auto subname, name.Submodule("extra"));
  EXPECT_EQ(subname.name(), "foo.bar.baz.extra");
  ASSERT_OK_AND_ASSIGN(subname, name.Subfunction("extra"));
  EXPECT_EQ(subname.name(), "foo.bar.baz::extra");
  EXPECT_RAISES(name.Submodule("1extra").status(), InvalidArgument);
  EXPECT_RAISES(name.Subfunction("1extra").status(), InvalidArgument);
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), name.function_names()),
            name.name());
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), {}), name.module_name());
  EXPECT_EQ(ScopeName::Recompose({}, name.function_names()), "");
}

TEST(ScopeName, ParseFunction) {
  ASSERT_OK_AND_ASSIGN(ScopeName name, ScopeName::Parse("::foo::bar::baz"));
  EXPECT_EQ(name.size(), 3);
  EXPECT_FALSE(name.empty());
  EXPECT_EQ(name.name(), "::foo::bar::baz");
  EXPECT_EQ(name.module_name(), "");
  EXPECT_EQ(name.function_name(), "foo::bar::baz");
  EXPECT_EQ(name.hash(), std::hash<std::string>()(name.name()));
  EXPECT_EQ(name.PrefixName(0), "");
  EXPECT_EQ(name.PrefixName(1), "::foo");
  EXPECT_EQ(name.PrefixName(2), "::foo::bar");
  EXPECT_EQ(name.PrefixName(3), name.name());
  EXPECT_EQ(name.PrefixName(4), name.name());
  ASSERT_OK_AND_ASSIGN(auto subname, name.Submodule("extra"));
  EXPECT_EQ(subname.name(), "extra::foo::bar::baz");
  ASSERT_OK_AND_ASSIGN(subname, name.Subfunction("extra"));
  EXPECT_EQ(subname.name(), "::foo::bar::baz::extra");
  EXPECT_RAISES(name.Submodule("1extra").status(), InvalidArgument);
  EXPECT_RAISES(name.Subfunction("1extra").status(), InvalidArgument);
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), name.function_names()),
            name.name());
  EXPECT_EQ(ScopeName::Recompose(name.module_names(), {}), "");
  EXPECT_EQ(ScopeName::Recompose({}, name.function_names()), name.name());
}

TEST(ScopeName, Empty) {
  ASSERT_OK_AND_ASSIGN(ScopeName name, ScopeName::Parse(""));
  EXPECT_EQ(name.size(), 0);
  EXPECT_TRUE(name.empty());
  EXPECT_EQ(name.name(), "");
  EXPECT_EQ(name.module_name(), "");
  EXPECT_EQ(name.function_name(), "");
  EXPECT_EQ(name.hash(), std::hash<std::string>()(""));
}

TEST(ScopedName, Parse) {
  ASSERT_OK_AND_ASSIGN(ScopedName name, ScopedName::Parse("foo.bar::baz.qux"));
  EXPECT_EQ(name.scope_name().name(), "foo.bar::baz");
  EXPECT_EQ(name.name(), "qux");
  EXPECT_EQ(name.full_name(), "foo.bar::baz.qux");

  pb::Identifier identifier;
  identifier.add_name("foo");
  identifier.add_name("bar");
  identifier.add_name("baz");
  ASSERT_OK_AND_ASSIGN(name, ScopedName::FromIdentifier(identifier));
  EXPECT_EQ(name.scope_name().name(), "foo.bar");
  EXPECT_EQ(name.name(), "baz");
  EXPECT_EQ(name.full_name(), "foo.bar.baz");
  EXPECT_RAISES(ScopedName::Parse("foo.bar::baz::qux").status(),
                InvalidArgument);
  ASSERT_OK_AND_ASSIGN(name, ScopedName::Parse("foo"));
  EXPECT_EQ(name.scope_name().name(), "");
  EXPECT_EQ(name.name(), "foo");
  EXPECT_EQ(name.full_name(), "foo");
  EXPECT_RAISES(ScopedName::Parse("foo.bar::baz::qux").status(),
                InvalidArgument);
}

TEST(ScopeName, IsPrefix) {
  ScopeName n1;
  ASSERT_OK_AND_ASSIGN(ScopeName n2, ScopeName::Parse("foo"));
  ASSERT_OK_AND_ASSIGN(ScopeName n3, ScopeName::Parse("foo_bar.baz"));
  ASSERT_OK_AND_ASSIGN(ScopeName n4, ScopeName::Parse("foo.bar"));
  ASSERT_OK_AND_ASSIGN(ScopeName n5, ScopeName::Parse("foo.bar.baz"));
  ASSERT_OK_AND_ASSIGN(ScopeName n6, ScopeName::Parse("foo.bar::baz"));
  ASSERT_OK_AND_ASSIGN(ScopeName n7, ScopeName::Parse("foo.bar::baz::qux"));
  const ScopeName* names[] = {&n1, &n2, &n3, &n4, &n5, &n6, &n7};
  const bool is_prefix[] = {
      true,  true,  true,  true,  true,  true,  true,  false, true,  false,
      true,  true,  true,  true,  false, false, true,  false, false, false,
      false, false, false, false, true,  true,  true,  true,  false, false,
      false, false, true,  false, false, false, false, false, false, false,
      true,  true,  false, false, false, false, false, false, true,
  };
  const size_t kNum = ABSL_ARRAYSIZE(names);
  static_assert(kNum * kNum == ABSL_ARRAYSIZE(is_prefix));
  for (size_t i = 0; i < kNum; ++i) {
    for (size_t j = 0; j < kNum; ++j) {
      EXPECT_EQ(names[i]->IsPrefixScope(*names[j]), is_prefix[i * kNum + j])
          << "For: " << i << " / " << j;
    }
  }
}

TEST(SopeName, Protos) {
  ASSERT_OK_AND_ASSIGN(auto scope_name, ScopeName::Parse("foo.bar::baz::qux"));
  auto proto = scope_name.ToProto();
  EXPECT_THAT(proto, EqualsProto(R"pb(
                module_name: "foo"
                module_name: "bar"
                function_name: "baz"
                function_name: "qux"
              )pb"));
  ASSERT_OK_AND_ASSIGN(auto scope_name2, ScopeName::FromProto(proto));
  EXPECT_EQ(scope_name.name(), scope_name2.name());
  pb::ScopeName p;
  ASSERT_OK_AND_ASSIGN(auto s, ScopeName::FromProto(p));
  EXPECT_TRUE(s.empty());
  p.add_module_name("x-");
  EXPECT_RAISES(ScopeName::FromProto(p), InvalidArgument);
  p.clear_module_name();
  p.add_function_name("x-");
  EXPECT_RAISES(ScopeName::FromProto(p), InvalidArgument);

  pb::ScopedName sproto;
  *sproto.mutable_scope_name() = p;
  EXPECT_RAISES(ScopedName::FromProto(sproto), InvalidArgument);
  *sproto.mutable_scope_name() = proto;
  EXPECT_RAISES(ScopedName::FromProto(sproto), InvalidArgument);
  sproto.set_name("some");
  ASSERT_OK_AND_ASSIGN(auto name, ScopedName::FromProto(sproto));
  EXPECT_EQ(name.name(), "some");
  EXPECT_EQ(name.full_name(), "foo.bar::baz::qux.some");
  EXPECT_THAT(name.ToProto(), EqualsProto(sproto));

  ScopedName sempty(
      std::make_shared<ScopeName>(ScopeName::Parse("foo.bar").value()), "");
  EXPECT_EQ(sempty.full_name(), "foo.bar");
}

}  // namespace analysis
}  // namespace nudl
