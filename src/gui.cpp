#include "gui.hpp"

#include <absl/status/statusor.h>
#include <dirent.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <string_view>

namespace {

class UINavBar : public NavBar {
 public:
  UINavBar() {
    // Allows buttons in border to remain in top left corner.
    border_.set_halign(Gtk::ALIGN_START);
    border_.set_valign(Gtk::ALIGN_START);

    // Insert in this order so the up button is at the right, and the back
    // button is at the left.
    border_.pack_start(up_button_, Gtk::PackOptions::PACK_SHRINK);
    border_.pack_start(forward_button_, Gtk::PackOptions::PACK_SHRINK);
    border_.pack_start(back_button_, Gtk::PackOptions::PACK_SHRINK);

    border_.set_border_width(20);
  }

  UINavBar(const UINavBar &) = delete;
  UINavBar(UINavBar &&) = delete;
  UINavBar &operator=(const UINavBar &) = delete;
  UINavBar &operator=(UINavBar &&) = delete;
  virtual ~UINavBar() {}

  // Allows sharing of navigation bar border to attach as child to GTK tree.
  Gtk::Box &GetBorder() { return border_; }

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
  Gtk::Box border_;
  Gtk::Button back_button_;
  Gtk::Button forward_button_;
  Gtk::Button up_button_;
};

class UISearchBar : public SearchBar {
 public:
  void OnFileToSearchEntered(
      std::function<void(std::string_view)> callback) override {}
};

}  // namespace

NavBar::NavBar() {}
NavBar::~NavBar() {}
SearchBar::SearchBar() {}
SearchBar::~SearchBar() {}

Window::Window(NavBar *nav_bar, SearchBar *search_bar)
    : Window(*nav_bar, *search_bar) {}
Window::Window(NavBar &nav_bar, SearchBar &search_bar)
    : navigate_buttons_(&nav_bar), file_search_bar_(&search_bar) {
  navigate_buttons_->OnBackButtonPress([this]() { this->GoBackDirectory(); });
  navigate_buttons_->OnUpButtonPress([this]() { this->GoUpDirectory(); });
  navigate_buttons_->OnForwardButtonPress(
      [this]() { this->GoForwardDirectory(); });
  file_search_bar_->OnFileToSearchEntered(
      [this](absl::string_view file_name) { this->SearchForFile(file_name); });
}

Window::~Window() {}

void Window::GoBackDirectory() {}
void Window::GoUpDirectory() {}
void Window::GoForwardDirectory() {}

dirent *Window::SearchForFile(std::string_view file_name) { return nullptr; }

NavBar &Window::GetNavBar() { return *navigate_buttons_.get(); }
SearchBar &Window::GetSearchBar() { return *file_search_bar_.get(); }

UIWindow::UIWindow() : ::Window(new UINavBar(), new UISearchBar()) {
  add(window_widgets_);

  // Insert the navigation bar at the top left of the window. Have to dynamic
  // cast it since it's stored as the base type, but we know it's for sure a
  // UINavBar, so no error-checking.
  auto &nav_bar = dynamic_cast<UINavBar &>(GetNavBar());
  window_widgets_.pack_start(nav_bar.GetBorder(),
                             Gtk::PackOptions::PACK_SHRINK);

  show_all();
}
