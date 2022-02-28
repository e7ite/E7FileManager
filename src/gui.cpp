#include "gui.hpp"

#include <dirent.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>

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

class UICurrentDirectoryBar : public CurrentDirectoryBar {
 public:
  UICurrentDirectoryBar() {
    // Required to make box not expand vertically.
    file_search_entry_box_.set_halign(Gtk::ALIGN_END);
    file_search_entry_box_.set_valign(Gtk::ALIGN_START);

    file_search_entry_box_.set_placeholder_text("File to enter...");

    file_search_entry_box_.set_size_request(50, 20);
  }

  UICurrentDirectoryBar(const UICurrentDirectoryBar &) = delete;
  UICurrentDirectoryBar(UICurrentDirectoryBar &&) = delete;
  UICurrentDirectoryBar &operator=(const UICurrentDirectoryBar &) = delete;
  UICurrentDirectoryBar &operator=(UICurrentDirectoryBar &&) = delete;
  virtual ~UICurrentDirectoryBar() {}

  void OnDirectoryChange(
      std::function<void(const Glib::ustring &, int *)> callback) override {}

  void OnFileToSearchEntered(
      std::function<void(const Glib::ustring &, int *)> callback) override {
    file_search_entry_box_.signal_insert_text().connect(callback);
  }

  Gtk::Entry &GetTextBox() { return file_search_entry_box_; }

 private:
  Gtk::Entry file_search_entry_box_;
};

}  // namespace

NavBar::NavBar() {}
NavBar::~NavBar() {}
CurrentDirectoryBar::CurrentDirectoryBar() {}
CurrentDirectoryBar::~CurrentDirectoryBar() {}

Window::Window(NavBar &nav_bar, CurrentDirectoryBar &directory_bar)
    : navigate_buttons_(&nav_bar), current_directory_bar_(&directory_bar) {
  navigate_buttons_->OnBackButtonPress([this]() { this->GoBackDirectory(); });
  navigate_buttons_->OnUpButtonPress([this]() { this->GoUpDirectory(); });
  navigate_buttons_->OnForwardButtonPress(
      [this]() { this->GoForwardDirectory(); });

  current_directory_bar_->OnFileToSearchEntered(
      [this](const Glib::ustring &file_name, [[maybe_unused]] int *) {
        Glib::UStringView file_name_view = file_name;
        this->SearchForFile(file_name_view);
      });
  current_directory_bar_->OnDirectoryChange(
      [this](const Glib::ustring &directory_name, [[maybe_unused]] int *) {
        Glib::UStringView directory_name_view = directory_name;
        this->UpdateDirectory(directory_name_view);
      });
}

Window::~Window() {}

void Window::GoBackDirectory() {}
void Window::GoUpDirectory() {}
void Window::GoForwardDirectory() {}

dirent *Window::SearchForFile(Glib::UStringView file_name) { return nullptr; }

bool Window::UpdateDirectory(Glib::UStringView new_directory) { return true; }

NavBar &Window::GetNavBar() { return *navigate_buttons_.get(); }
CurrentDirectoryBar &Window::GetDirectoryBar() {
  return *current_directory_bar_.get();
}

UIWindow::UIWindow() : ::Window(*new UINavBar(), *new UICurrentDirectoryBar()) {
  add(window_widgets_);

  // Insert the navigation bar at the top left of the window. Have to dynamic
  // cast it since it's stored as the base type, but we know it's for sure a
  // UINavBar, so no error-checking.
  auto &nav_bar = dynamic_cast<UINavBar &>(GetNavBar());
  window_widgets_.pack_start(nav_bar.GetBorder(),
                             Gtk::PackOptions::PACK_SHRINK);

  auto &directory_bar =
      dynamic_cast<UICurrentDirectoryBar &>(GetDirectoryBar());
  window_widgets_.pack_end(directory_bar.GetTextBox(),
                           Gtk::PackOptions::PACK_SHRINK);

  show_all();
}
