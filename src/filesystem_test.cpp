#include "filesystem.hpp"

#include <absl/base/casts.h>
#include <absl/memory/memory.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/strings/str_split.h>
#include <absl/types/any.h>
#include <absl/utility/utility.h>
#include <glibmm/ustring.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtkmm/window.h>

#include <array>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <utility>

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {

inline const ::absl::Status& GetStatus(const ::absl::Status& status) {
  return status;
}

template <typename T>
inline const ::absl::Status& GetStatus(const ::absl::StatusOr<T>& status) {
  return status.status();
}

// Monomorphic implementation of matcher IsOkAndHolds(m).  StatusOrType is a
// reference to StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  typedef
      typename std::remove_reference<StatusOrType>::type::value_type value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const value_type&>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      StatusOrType actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value "
                       << ::testing::PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const value_type&> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  StatusOrType can be either StatusOr<T> or a
  // reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {  // NOLINT
    return ::testing::Matcher<StatusOrType>(
        new IsOkAndHoldsMatcherImpl<const StatusOrType&>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
};

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(T actual_value,
                       ::testing::MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::Matcher<T>(new MonoIsOkMatcherImpl<T>());
  }
};

// Macros for testing the results of functions that return absl::Status or
// absl::StatusOr<T> (for any type T).
#define EXPECT_OK(expression) EXPECT_THAT(expression, IsOk())

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type> IsOkAndHolds(
    InnerMatcher&& inner_matcher) {
  return IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline IsOkMatcher IsOk() { return IsOkMatcher(); }

TEST(MockFileSystemTest, EmptyCheck) {
  MockFileSystem mock_fs({});

  EXPECT_THAT(mock_fs.GetRoot().GetFiles(), IsEmpty());
}

TEST(MockFileSystemTest, HasOneFile) {
  MockFileSystem mock_fs({new MockFile("meow.txt")});

  EXPECT_THAT(mock_fs.GetRoot().GetFiles(), SizeIs(1));

  EXPECT_THAT(mock_fs.GetRoot().GetFiles()[0]->GetName(), StrEq("meow.txt"));
}

TEST(MockFileSystemTest, HasOneDirectory) {
  MockFileSystem mock_fs({new MockDirectory("dir", {})});

  EXPECT_THAT(mock_fs.GetRoot().GetFiles(), SizeIs(1));

  EXPECT_THAT(mock_fs.GetRoot().GetFiles()[0]->GetName(), StrEq("dir"));
}

TEST(MockFileSystemTest, DynamicCastToMockDirectorySuccess) {
  MockFileSystem mock_fs({new MockDirectory("dir", {})});

  EXPECT_THAT(
      dynamic_cast<const MockDirectory*>(mock_fs.GetRoot().GetFiles()[0]),
      NotNull());
}

TEST(MockFileSystemTest, DynamicCastToMockDirectoryFail) {
  MockFileSystem mock_fs({new MockFile("cat")});

  EXPECT_THAT(
      dynamic_cast<const MockDirectory*>(mock_fs.GetRoot().GetFiles()[0]),
      IsNull());
}

TEST(MockFileSystemTest, ErrorOnEmptyPath) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory("dir", {new MockFile("meow.txt")})});

  // Negate result, since we can't explicitly check for exact statuses with
  // absl::Status right now.
  EXPECT_THAT(mock_fs.GetDirectoryFiles(""), Not(IsOk()));
}

TEST(MockFileSystemTest, ErrorOnRelativePath) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory("dir", {new MockFile("meow.txt")})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("dir/"), Not(IsOk()));
}

TEST(MockFileSystemTest, GetFilesFromRoot) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory("dir", {new MockFile("meow.txt")})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/"),
              IsOkAndHolds(UnorderedElementsAre("dir", "file.txt")));
}

TEST(MockFileSystemTest, AccessNestedDirectory) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory("dir", {new MockFile("meow.txt")})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir"),
              IsOkAndHolds(UnorderedElementsAre("meow.txt")));
}

TEST(MockFileSystemTest, AccessDoubleNestedDirectory) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory(
           "dir",
           {new MockDirectory("nesteddir", {new MockFile("ruff.txt")})})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir/nesteddir"),
              IsOkAndHolds(UnorderedElementsAre("ruff.txt")));
}

TEST(MockFileSystemTest, AccessNestedFileAsDir) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory(
           "dir",
           {new MockFile("lol.txt"),
            new MockDirectory("nesteddir", {new MockFile("ruff.txt")})})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir/lol.txt"),
              IsOkAndHolds(UnorderedElementsAre("lol.txt")));
}

TEST(MockFileSystemTest, FailOnIncorrectNestedMiddleDir) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory(
           "dir", {new MockDirectory(
                      "nesteddir",
                      {new MockDirectory("intoodeep",
                                         {new MockFile("hahaha.txt")})})})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir/nesteddirr/intoodeep"),
              Not(IsOk()));
}

TEST(MockFileSystemTest, AccessTripleNestedDir) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory(
           "dir",
           {new MockDirectory(
               "nesteddir",
               {new MockDirectory("intoodeep",
                                  {new MockFile("hahaha.txt"),
                                   new MockFile("wutwutwutwut.jpg")})})})});

  EXPECT_THAT(
      mock_fs.GetDirectoryFiles("/dir/nesteddir/intoodeep"),
      IsOkAndHolds(UnorderedElementsAre("hahaha.txt", "wutwutwutwut.jpg")));
}

TEST(MockFileSystemTest, FailOnAccessNonExistingDoubleNestedDir) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory(
           "dir",
           {new MockDirectory("nesteddir", {new MockFile("ruff.txt")})})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir/stuff"), Not(IsOk()));
}

TEST(MockFileSystemTest, AccessNestedDirectoryWithExtraForwardSlash) {
  MockFileSystem mock_fs(
      {new MockFile("file.txt"),
       new MockDirectory("dir", {new MockFile("meow.txt")})});

  EXPECT_THAT(mock_fs.GetDirectoryFiles("/dir/"),
              IsOkAndHolds(UnorderedElementsAre("meow.txt")));
}

}  // namespace