#include "gui.hpp"

#include <absl/base/casts.h>
#include <absl/memory/memory.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/any.h>
#include <absl/utility/utility.h>
#include <dirent.h>
#include <glibmm/ustring.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtkmm/window.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <stack>
#include <type_traits>
#include <utility>

#include "filesystem.hpp"

using ::testing::_;
using ::testing::Exactly;
using ::testing::InitGoogleTest;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnNull;

namespace Glib {
void PrintTo(const Glib::ustring& gtkmm_string, std::ostream* os) {
  *os << (gtkmm_string.is_ascii() ? "ASCII: " : "Not ASCII: ")
      << static_cast<std::string>(gtkmm_string);
}
}  // namespace Glib

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

  MOCK_METHOD(void, SetDisplayedDirectory, (const Glib::ustring& new_directory),
              (override));

  void OnDirectoryChange(
      std::function<void(const Glib::ustring&, int*)> callback) override {
    directory_changed_callback_ = callback;
  }

  void SimulateDirectoryChange(const Glib::ustring& directory_name) {
    directory_changed_callback_(directory_name, nullptr);
  }

  void OnFileToSearchEntered(
      std::function<void(const Glib::ustring&, int*)> callback) override {
    file_typed_callback_ = callback;
  }

  void SimulateFileToSearchEntered(const Glib::ustring& file_name) {
    file_typed_callback_(file_name, nullptr);
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
  MockWindow(NavBar& nav_bar, CurrentDirectoryBar& current_directory_bar,
             DirectoryFilesView& directory_files_view, FileSystem& file_system)
      : Window(nav_bar, current_directory_bar, directory_files_view,
               file_system) {}
  virtual ~MockWindow() {}

  MockWindow(const MockWindow&) = delete;
  MockWindow& operator=(const MockWindow&) = delete;
  MockWindow& operator=(MockWindow&&) = delete;
  MockWindow(MockWindow&&) = delete;

  MOCK_METHOD(void, GoBackDirectory, (), (override));
  MOCK_METHOD(void, GoForwardDirectory, (), (override));
  MOCK_METHOD(void, GoUpDirectory, (), (override));
  MOCK_METHOD(void, HandleFullDirectoryChange,
              (const Glib::ustring& new_directory), (override));

  MOCK_METHOD(dirent*, SearchForFile, (const Glib::ustring& file_name),
              (override));

  MOCK_METHOD(void, ShowFileDetails, (const Glib::ustring& file_name),
              (override));

  void CallGoBackDirectory() { Window::GoBackDirectory(); }
  void CallGoForwardDirectory() { Window::GoForwardDirectory(); }
  void CallGoUpDirectory() { Window::GoUpDirectory(); }
  void CallFullDirectoryChange(const Glib::ustring& new_directory) {
    Window::HandleFullDirectoryChange(new_directory);
  }

  void RefreshWindowComponents() override {
    GetDirectoryBar().SetDisplayedDirectory(GetCurrentDirectory());
    GetDirectoryFilesView().AddFile("meow.txt");
  }
};

class WindowTest : public ::testing::Test {
 protected:
  WindowTest()
      : mock_nav_bar_(*new MockNavBar()),
        mock_current_directory_bar_(*new MockCurrentDirectoryBar()),
        mock_directory_files_view_(*new MockDirectoryFilesView()),
        mock_file_system_(*new MockFileSystem(
            {new MockFile("meow.txt"),
             new MockDirectory(
                 "dir",
                 {new MockDirectory("nesteddir",
                                    {new MockFile("lmao.txt"),
                                     new MockFile("nameabettertest.cpp"),
                                     new MockFile("whyyoualwayslying.lol")})}),
             new MockDirectory("meow", {})})),
        mock_window_(mock_nav_bar_, mock_current_directory_bar_,
                     mock_directory_files_view_, mock_file_system_) {}

  MockNavBar& mock_nav_bar_;
  MockCurrentDirectoryBar& mock_current_directory_bar_;
  MockDirectoryFilesView& mock_directory_files_view_;
  MockFileSystem& mock_file_system_;
  MockWindow mock_window_;
};

TEST_F(WindowTest, EnsureStartsAtRoot) {
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureBackButtonRequestGoesBackDirectory) {
  EXPECT_CALL(mock_window_, GoBackDirectory()).Times(Exactly(1));
  mock_nav_bar_.SimulateBackButtonPress();
}

TEST_F(WindowTest, EnsureBackButtonGoesBackOneDir) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));

  mock_window_.Window::UpdateDirectory("/dir");

  mock_nav_bar_.SimulateBackButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureBackButtonDoesNotGoBackWithoutHistory) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));

  mock_nav_bar_.SimulateBackButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureForwardButtonDoesNotGoBackWithoutHistory) {
  EXPECT_CALL(mock_window_, GoForwardDirectory())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(&mock_window_,
                                  &MockWindow::CallGoForwardDirectory));

  mock_nav_bar_.SimulateForwardButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureForwardButtonGoesToDirBeforeBackButtonPressed) {
  EXPECT_CALL(mock_window_, GoForwardDirectory())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(&mock_window_,
                                  &MockWindow::CallGoForwardDirectory));

  mock_window_.Window::UpdateDirectory("/dir");

  mock_nav_bar_.SimulateBackButtonPress();

  mock_nav_bar_.SimulateForwardButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");
}

TEST_F(WindowTest, EnsureBackFromTwoDirectoryNavigationsWorks) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(2))
      .WillRepeatedly(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));

  mock_window_.Window::UpdateDirectory("/dir");
  mock_window_.Window::UpdateDirectory("/dir/nesteddir");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, NavigateTwoDirsBackBackForwardBack) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(3))
      .WillRepeatedly(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));
  EXPECT_CALL(mock_window_, GoForwardDirectory())
      .Times(Exactly(1))
      .WillRepeatedly(InvokeWithoutArgs(&mock_window_,
                                        &MockWindow::CallGoForwardDirectory));

  mock_window_.Window::UpdateDirectory("/dir");
  mock_window_.Window::UpdateDirectory("/dir/nesteddir");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

  mock_nav_bar_.SimulateForwardButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureUpButtonRemovesOneDir) {
  EXPECT_CALL(mock_window_, GoUpDirectory())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoUpDirectory));

  mock_window_.Window::UpdateDirectory("/dir");

  mock_nav_bar_.SimulateUpButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, EnsureUpButtonDoesNotWorkOnRoot) {
  EXPECT_CALL(mock_window_, GoUpDirectory())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoUpDirectory));

  mock_nav_bar_.SimulateUpButtonPress();

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, UpButtonShouldClearHistory) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));
  EXPECT_CALL(mock_window_, GoForwardDirectory())
      .Times(Exactly(1))
      .WillRepeatedly(InvokeWithoutArgs(&mock_window_,
                                        &MockWindow::CallGoForwardDirectory));
  EXPECT_CALL(mock_window_, GoUpDirectory())
      .Times(Exactly(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoUpDirectory));

  mock_window_.Window::UpdateDirectory("/dir");
  mock_window_.Window::UpdateDirectory("/dir/nesteddir");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

  mock_nav_bar_.SimulateUpButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

  mock_nav_bar_.SimulateForwardButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
}

TEST_F(WindowTest, ChangeDirFromDirectoryBarShouldClearHistory) {
  EXPECT_CALL(mock_window_, GoBackDirectory())
      .Times(Exactly(2))
      .WillRepeatedly(
          InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));
  EXPECT_CALL(mock_window_, GoForwardDirectory())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(&mock_window_,
                                  &MockWindow::CallGoForwardDirectory));
  EXPECT_CALL(mock_window_, UpdateDirectory(Glib::ustring("/dir/nesteddir")))
      .Times(Exactly(1))
      .WillOnce(Invoke(&mock_window_, &MockWindow::CallUpdateDirectory));

  mock_window_.Window::UpdateDirectory("/dir");            // NOLINT
  mock_window_.Window::UpdateDirectory("/dir/nesteddir");  // NOLINT

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

  mock_nav_bar_.SimulateBackButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

  mock_current_directory_bar_.SimulateDirectoryChange("/dir/nesteddir");

  mock_nav_bar_.SimulateForwardButtonPress();
  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/nesteddir/");
}

// TEST_F(WindowTest, ChangingDirectoryUltimateBoss) {
//   EXPECT_CALL(mock_window_, GoBackDirectory())
//       .Times(Exactly(6))
//       .WillRepeatedly(
//           InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoBackDirectory));
//   EXPECT_CALL(mock_window_, GoForwardDirectory())
//       .Times(Exactly(5))
//       .WillRepeatedly(InvokeWithoutArgs(&mock_window_,
//                                         &MockWindow::CallGoForwardDirectory));
//     EXPECT_CALL(mock_window_, GoUpDirectory())
//       .Times(Exactly(1))
//       .WillRepeatedly(
//           InvokeWithoutArgs(&mock_window_, &MockWindow::CallGoUpDirectory));
//   EXPECT_CALL(mock_window_, UpdateDirectory(Glib::ustring("/meow")))
//       .Times(Exactly(1))
//       .WillOnce(Invoke(&mock_window_, &MockWindow::CallUpdateDirectory));
//   EXPECT_CALL(mock_window_, UpdateDirectory(Glib::ustring("/dir/nesteddir/")))
//       .Times(Exactly(1))
//       .WillOnce(Invoke(&mock_window_, &MockWindow::CallUpdateDirectory));
//   EXPECT_CALL(mock_current_directory_bar_,
//               SetDisplayedDirectory(Glib::ustring("/dir/nesteddir/")))  // NOLINT
//       .Times(Exactly(1))
//       .WillOnce(Return(true));
//   EXPECT_CALL(mock_current_directory_bar_,
//               SetDisplayedDirectory(Glib::ustring("/meow")))  // NOLINT
//       .Times(Exactly(1))
//       .WillOnce(Return(true));

//   mock_window_.Window::UpdateDirectory("/dir");
//   mock_window_.Window::UpdateDirectory("/dir/nesteddir");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

//   mock_nav_bar_.SimulateForwardButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

//   mock_nav_bar_.SimulateForwardButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

//   mock_nav_bar_.SimulateForwardButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/nesteddir/");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

//   mock_current_directory_bar_.SimulateDirectoryChange("/meow");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");

//   mock_nav_bar_.SimulateForwardButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/meow/");

//   mock_window_.SimulateDirectoryChange("/dir/nesteddir/");

//   mock_nav_bar_.SimulateBackButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/meow/");

//   mock_nav_bar_.SimulateUpButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");

//   mock_nav_bar_.SimulateForwardButtonPress();
//   ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/");
// }

TEST_F(WindowTest, EnsureForwardButtonRequestReceived) {
  EXPECT_CALL(mock_window_, GoForwardDirectory()).Times(Exactly(1));
  mock_nav_bar_.SimulateForwardButtonPress();
}

TEST_F(WindowTest, EnsureUpButtonRequestReceived) {
  EXPECT_CALL(mock_window_, GoUpDirectory()).Times(Exactly(1));
  mock_nav_bar_.SimulateUpButtonPress();
}

TEST_F(WindowTest, EnsureFileIsSearchedFor) {
  EXPECT_CALL(mock_window_, SearchForFile(Glib::ustring("hello.txt")))
      .Times(Exactly(1))
      .WillOnce(ReturnNull());
  // Don't lint this because clang-tidy claims there will be a memory leak here,
  // but we don't care since this is tests.
  mock_current_directory_bar_.SimulateFileToSearchEntered(
      "hello.txt");  // NOLINT
}

TEST_F(WindowTest, EnsureWindowDirectoryUpdatesUponDirectoryBarChange) {
  EXPECT_CALL(mock_window_, UpdateDirectory(Glib::ustring("/dir/")))
      .Times(Exactly(1))
      .WillOnce(Invoke(&mock_window_, &MockWindow::CallUpdateDirectory))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_current_directory_bar_,
              SetDisplayedDirectory(Glib::ustring("/dir/")))  // NOLINT
      .Times(Exactly(1))
      .WillOnce(Return(true));

  mock_current_directory_bar_.SimulateDirectoryChange("/dir/");  // NOLINT

  ASSERT_STREQ(mock_window_.GetCurrentDirectory().c_str(), "/dir/");
}

TEST_F(WindowTest, EnsureDirectoryWidgetUpdatesUponRequestFromWindow) {
  EXPECT_CALL(mock_window_,
              HandleFullDirectoryChange(Glib::ustring("/dir")))  // NOLINT
      .Times(Exactly(1))
      .WillOnce(Invoke(&mock_window_, &MockWindow::CallFullDirectoryChange));
  EXPECT_CALL(mock_current_directory_bar_,
              SetDisplayedDirectory(Glib::ustring("/dir/")))  // NOLINT
      .Times(Exactly(1))
      .WillOnce(Return());

  mock_directory_files_view_.SimulateDirectoryClick("dir");  // NOLINT
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}