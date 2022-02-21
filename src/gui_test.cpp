#include "gui.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>

using ::testing::Exactly;
using ::testing::InitGoogleTest;

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

// Acts as regular window, and is used to ensure methods of Window are
// invoked and state changes as expected.
class MockWindow : public Window {
 public:
  MockWindow() : Window(new MockNavBar()) {}
  virtual ~MockWindow() {}

  MockWindow(const MockWindow&) = delete;
  MockWindow& operator=(const MockWindow&) = delete;
  MockWindow& operator=(MockWindow&&) = delete;
  MockWindow(MockWindow&&) = delete;

  MOCK_METHOD(void, GoBackDirectory, (), (override));
  MOCK_METHOD(void, GoForwardDirectory, (), (override));
  MOCK_METHOD(void, GoUpDirectory, (), (override));
};

TEST(WindowTest, EnsureBackButtonResponseReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoBackDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateBackButtonPress();
}

TEST(WindowTest, EnsureForwardButtonResponseReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoForwardDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateForwardButtonPress();
}

TEST(WindowTest, EnsureUpButtonResponseReceived) {
  MockWindow mock_window;
  EXPECT_CALL(mock_window, GoUpDirectory()).Times(Exactly(1));
  auto* mock_nav_bar = dynamic_cast<MockNavBar*>(&mock_window.GetNavBar());
  ASSERT_TRUE(mock_nav_bar != nullptr);
  mock_nav_bar->SimulateUpButtonPress();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}