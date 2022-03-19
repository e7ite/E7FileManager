#include "gui.hpp"

#include <absl/base/casts.h>
#include <absl/memory/memory.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/any.h>
#include <absl/utility/utility.h>
#include <dirent.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtkmm/window.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

using ::testing::_;
using ::testing::Exactly;
using ::testing::InitGoogleTest;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnNull;

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

// Imitates a NavBar and can be used to ensure button presses are handled
// correctly.
class MockNavBar : public NavBar {
 public:
  MockNavBar() : NavBar() {}

  MockNavBar(const MockNavBar&) = delete;
  MockNavBar(MockNavBar&&) = delete;
  MockNavBar& operator=(const MockNavBar&) = delete;
  MockNavBar& operator=(MockNavBar&&) = delete;
  virtual ~MockNavBar() {}

  void OnBackButtonPress(std::function<void()> callback) override {
    back_button_callback_ = callback;
  }
  void OnForwardButtonPress(std::function<void()> callback) override {
    forward_button_callback_ = callback;
  }
  void OnUpButtonPress(std::function<void()> callback) override {
    up_button_callback_ = callback;
  }

  void SimulateBackButtonPress() { back_button_callback_(); }
  void SimulateForwardButtonPress() { forward_button_callback_(); }
  void SimulateUpButtonPress() { up_button_callback_(); }

 private:
  std::function<void()> back_button_callback_;
  std::function<void()> forward_button_callback_;
  std::function<void()> up_button_callback_;
};

// Acts as the display of the current directory and the file search bar. Can
// send fake responses imititating calls from GTKMM, to mimic a user requesting
// a change to the current directory or a file search.
class MockCurrentDirectoryBar : public CurrentDirectoryBar {
 public:
  MockCurrentDirectoryBar() : CurrentDirectoryBar() {}

  virtual ~MockCurrentDirectoryBar() {}
  MockCurrentDirectoryBar(const MockCurrentDirectoryBar&) = delete;
  MockCurrentDirectoryBar(MockCurrentDirectoryBar&&) = delete;
  MockCurrentDirectoryBar& operator=(const MockCurrentDirectoryBar&) = delete;
  MockCurrentDirectoryBar& operator=(MockCurrentDirectoryBar&&) = delete;

  MOCK_METHOD(bool, SetDisplayedDirectory, (Glib::UStringView new_directory),
              (override));

  void OnDirectoryChange(
      std::function<void(const Glib::ustring&, int*)> callback) override {
    directory_changed_callback_ = callback;
  }

  void SimulateDirectoryChange(std::string_view file_name) {
    directory_changed_callback_(Glib::ustring(file_name.data()), nullptr);
  }

  void OnFileToSearchEntered(
      std::function<void(const Glib::ustring&, int*)> callback) override {
    file_typed_callback_ = callback;
  }

  void SimulateFileToSearchEntered(std::string_view file_name) {
    file_typed_callback_(Glib::ustring(file_name.data()), nullptr);
  }

 private:
  std::function<void(const Glib::ustring&, int*)> file_typed_callback_;
  std::function<void(const Glib::ustring&, int*)> directory_changed_callback_;
};

class MockDirectoryFilesView : public DirectoryFilesView {
 public:
  MockDirectoryFilesView() {}

  virtual ~MockDirectoryFilesView() {}
  MockDirectoryFilesView(const MockDirectoryFilesView&) = delete;
  MockDirectoryFilesView(MockDirectoryFilesView&&) = delete;
  MockDirectoryFilesView& operator=(const MockDirectoryFilesView&) = delete;
  MockDirectoryFilesView& operator=(MockDirectoryFilesView&&) = delete;

  void OnFileClick(
      std::function<void(const Glib::ustring&)> callback) override {
    file_clicked_callback_ = callback;
  }

  void OnDirectoryClick(
      std::function<void(const Glib::ustring&)> callback) override {
    directory_clicked_callback_ = callback;
  }

  void SimulateFileClick(const Glib::ustring& file_name) {
    file_clicked_callback_(file_name);
  }

  void SimulateDirectoryClick(const Glib::ustring& directory_name) {
    directory_clicked_callback_(directory_name);
  }

 private:
  std::function<void(const Glib::ustring&)> file_clicked_callback_;
  std::function<void(const Glib::ustring&)> directory_clicked_callback_;
};

// Acts as regular window, and is used to ensure methods of Window are invoked
// and state changes as expected.
class MockWindow : public Window {
 public:
  MockWindow()
      : Window(*new MockNavBar(), *new MockCurrentDirectoryBar(),
               *new MockDirectoryFilesView()) {}
  virtual ~MockWindow() {}

  MockWindow(const MockWindow&) = delete;
  MockWindow& operator=(const MockWindow&) = delete;
  MockWindow& operator=(MockWindow&&) = delete;
  MockWindow(MockWindow&&) = delete;

  MOCK_METHOD(void, GoBackDirectory, (), (override));
  MOCK_METHOD(void, GoForwardDirectory, (), (override));
  MOCK_METHOD(void, GoUpDirectory, (), (override));

  MOCK_METHOD(dirent*, SearchForFile, (Glib::UStringView file_name),
              (override));

  MOCK_METHOD(bool, UpdateDirectory, (Glib::UStringView new_directory),
              (override));

  MOCK_METHOD(void, ShowFileDetails, (Glib::UStringView file_name), (override));

  void SimulateDirectoryChange(Glib::UStringView new_directory) {
    GetDirectoryBar().SetDisplayedDirectory(new_directory);
  }
};

TEST(WindowTest, EnsureBackButtonRequestReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoBackDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateBackButtonPress();
}

TEST(WindowTest, EnsureForwardButtonRequestReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoForwardDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateForwardButtonPress();
}

TEST(WindowTest, EnsureUpButtonRequestReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoUpDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateUpButtonPress();
}

TEST(WindowTest, EnsureFileIsSearchedFor) {
  MockWindow mock_window;
  // Expect anything file name for now, since there is no
  // operator==(Glib::UStringView, Glib::UStringView). Fix later.
  EXPECT_CALL(mock_window, SearchForFile(_))
      .Times(Exactly(1))
      .WillOnce(ReturnNull());
  auto* mock_directory_bar =
      dynamic_cast<MockCurrentDirectoryBar*>(&mock_window.GetDirectoryBar());
  ASSERT_TRUE(mock_directory_bar != nullptr);
  mock_directory_bar->SimulateFileToSearchEntered("hello.txt");
}

TEST(WindowTest, EnsureDirectoryChangeRequestReceived) {
  MockWindow mock_window;
  // Expect anything directory name for now, since there is no
  // operator==(Glib::UStringView, Glib::UStringView). Fix later.
  EXPECT_CALL(mock_window, UpdateDirectory(_))
      .Times(Exactly(1))
      .WillOnce(Return(true));
  auto* mock_directory_bar =
      dynamic_cast<MockCurrentDirectoryBar*>(&mock_window.GetDirectoryBar());
  ASSERT_TRUE(mock_directory_bar != nullptr);
  mock_directory_bar->SimulateDirectoryChange("hello.txt");
}

TEST(WindowTest, EnsureDirectoryWidgetReceivesUpdatedDirectory) {
  MockWindow mock_window;
  auto* mock_directory_bar =
      dynamic_cast<MockCurrentDirectoryBar*>(&mock_window.GetDirectoryBar());
  ASSERT_TRUE(mock_directory_bar != nullptr);
  EXPECT_CALL(*mock_directory_bar, SetDisplayedDirectory(_))
      .Times(Exactly(1))
      .WillOnce(Return(true));
  mock_window.SimulateDirectoryChange("/tmp/directory/");
}

TEST(WindowTest, EnsureFileClickReceivedOnFileSelection) {
  MockWindow mock_window;
  auto* mock_directory_files_view = dynamic_cast<MockDirectoryFilesView*>(
      &mock_window.GetDirectoryFilesView());
  ASSERT_TRUE(mock_directory_files_view != nullptr);
  EXPECT_CALL(mock_window, ShowFileDetails(_)).Times(Exactly(1));
  mock_directory_files_view->SimulateFileClick("cat.txt");
}

TEST(WindowTest, EnsureDirectoryUpdateOnDirectorySelection) {
  MockWindow mock_window;
  auto* mock_directory_files_view = dynamic_cast<MockDirectoryFilesView*>(
      &mock_window.GetDirectoryFilesView());
  ASSERT_TRUE(mock_directory_files_view != nullptr);
  EXPECT_CALL(mock_window, UpdateDirectory(_))
      .Times(Exactly(1))
      .WillOnce(Return(true));
  mock_directory_files_view->SimulateDirectoryClick("cat.txt");
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}