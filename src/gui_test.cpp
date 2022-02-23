#include "gui.hpp"

#include <dirent.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>

#include <string_view>

using ::testing::_;
using ::testing::Exactly;
using ::testing::InitGoogleTest;
using ::testing::ReturnNull;

namespace {

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

  // Registers a callback to invoke when each button is pressed. If you wish to
  // pass in arguments, wrap the call in a lambda.
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

// Acts as the search bar, and is used to ensure methods of Window are invoked
// for when the user enters text to search for.
class MockSearchBar : public SearchBar {
 public:
  MockSearchBar() : SearchBar() {}
  virtual ~MockSearchBar() {}
  MockSearchBar(const MockSearchBar&) = delete;
  MockSearchBar(MockSearchBar&&) = delete;
  MockSearchBar& operator=(const MockSearchBar&) = delete;
  MockSearchBar& operator=(MockSearchBar&&) = delete;

  void OnFileToSearchEntered(
      std::function<void(const Glib::ustring&, int*)> callback) override {
    file_typed_callback_ = callback;
  }

  void SimulateFileToSearchEntered(std::string_view file_name) {
    file_typed_callback_(Glib::ustring(file_name.data()), nullptr);
  }

 private:
  std::function<void(const Glib::ustring&, int*)> file_typed_callback_;
};

// Acts as regular window, and is used to ensure methods of Window are invoked 
// and state changes as expected.
class MockWindow : public Window {
 public:
  MockWindow() : Window(new MockNavBar(), new MockSearchBar()) {}
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
  //ß operator==(Glib::UStringView, Glib::UStringView). Fix later.
  EXPECT_CALL(mock_window, SearchForFile(_))
      .Times(Exactly(1))
      .WillOnce(ReturnNull());
  auto* mock_search_bar =
      dynamic_cast<MockSearchBar*>(&mock_window.GetSearchBar());
  ASSERT_TRUE(mock_search_bar != nullptr);
  mock_search_bar->SimulateFileToSearchEntered("hello.txt");
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}