#include "gui.hpp"

#include <gtkmm/button.h>

#include <functional>
#include <memory>

namespace {

class UINavBar : public NavBar {
 public:
  UINavBar() {}

  UINavBar(const UINavBar &) = delete;
  UINavBar(UINavBar &&) = delete;
  UINavBar &operator=(const UINavBar &) = delete;
  UINavBar &operator=(UINavBar &&) = delete;
  virtual ~UINavBar() {}

  void OnBackButtonPress(std::function<void()> callback) override {
    back_button_.signal_clicked().connect(callback);
  }
  void OnForwardButtonPress(std::function<void()> callback) override {
    forward_button_.signal_clicked().connect(callback);
  }
  void OnUpButtonPress(std::function<void()> callback) override {
    up_button_.signal_clicked().connect(callback);
  }

 private:
  Gtk::Button back_button_;
  Gtk::Button forward_button_;
  Gtk::Button up_button_;
};

}  // namespace

NavBar::NavBar() {}
NavBar::~NavBar() {}

Window::Window(NavBar *nav_bar) : Window(*nav_bar) {}
Window::Window(NavBar &nav_bar) : navigate_buttons_(&nav_bar) {
  navigate_buttons_->OnBackButtonPress([this]() { this->GoBackDirectory(); });
  navigate_buttons_->OnUpButtonPress([this]() { this->GoUpDirectory(); });
  navigate_buttons_->OnForwardButtonPress(
      [this]() { this->GoForwardDirectory(); });
}

Window::~Window() {}

void Window::GoBackDirectory() {}
void Window::GoUpDirectory() {}
void Window::GoForwardDirectory() {}

NavBar &Window::GetNavBar() { return *navigate_buttons_.get(); }
