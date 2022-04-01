#include "gui.hpp"

#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <dirent.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <stack>

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
    entry_box_border_.set_halign(Gtk::ALIGN_END);
    entry_box_border_.set_valign(Gtk::ALIGN_START);
    entry_box_border_.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);

    entry_box_border_.pack_start(file_search_entry_box_);
    entry_box_border_.pack_start(current_directory_entry_box_);

    file_search_entry_box_.set_placeholder_text("File to search...");

    file_search_entry_box_.set_size_request(50, 20);

    current_directory_entry_box_.set_size_request(50, 20);
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

  void SetDisplayedDirectory(const Glib::ustring &new_directory) override {
    current_directory_entry_box_.set_text(new_directory);
  }

  Gtk::Box &GetBorder() { return entry_box_border_; }

 private:
  Gtk::Box entry_box_border_;
  Gtk::Entry file_search_entry_box_;
  Gtk::Entry current_directory_entry_box_;
};

class UIDirectoryFilesView : public DirectoryFilesView {
 public:
  UIDirectoryFilesView() {
    file_entry_widgets_.set_halign(Gtk::ALIGN_START);
    file_entry_widgets_.set_valign(Gtk::ALIGN_START);

    // Allows adding widgets on bottom of each other.
    file_entry_widgets_.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);

    // Allows window to stay on the bottom right of the main window.
    file_entries_window_.set_halign(Gtk::ALIGN_END);
    file_entries_window_.set_valign(Gtk::ALIGN_END);
    file_entries_window_.set_hexpand(true);
    file_entries_window_.set_vexpand(true);

    file_entries_window_.set_border_width(10);
    file_entries_window_.set_size_request(/*width=*/400, /*height=*/450);

    // Create horizontal scroll bars when needed, and vertical scroll bar
    // always.
    file_entries_window_.set_policy(/*hscrollbar_policy=*/Gtk::POLICY_AUTOMATIC,
                                    /*vscrollbar_policy=*/Gtk::POLICY_ALWAYS);

    file_entries_window_.add(file_entry_widgets_);
  }

  UIDirectoryFilesView(const UIDirectoryFilesView &) = delete;
  UIDirectoryFilesView(UIDirectoryFilesView &&) = delete;
  UIDirectoryFilesView &operator=(const UIDirectoryFilesView &) = delete;
  UIDirectoryFilesView &operator=(UIDirectoryFilesView &&) = delete;
  virtual ~UIDirectoryFilesView() {}

  void OnFileClick(
      std::function<void(const Glib::ustring &)> callback) override {}
  void OnDirectoryClick(
      std::function<void(const Glib::ustring &)> callback) override {}

  Gtk::ScrolledWindow &GetWindow() { return file_entries_window_; }

 private:
  Gtk::ScrolledWindow file_entries_window_;
  Gtk::Box file_entry_widgets_;
};

Glib::ustring RemoveLastDirectoryFromPath(const Glib::ustring &full_path,
                                          const Glib::ustring &last_path) {
  std::string path_to_clean = full_path;
  std::string suffix_to_remove = last_path + "/";
  absl::string_view cleaned_dir =
      absl::StripSuffix(path_to_clean, suffix_to_remove);
  return Glib::ustring(cleaned_dir.data(), cleaned_dir.size());
}

}  // namespace

NavBar::NavBar() {}
NavBar::~NavBar() {}
CurrentDirectoryBar::CurrentDirectoryBar() {}
CurrentDirectoryBar::~CurrentDirectoryBar() {}
DirectoryFilesView::DirectoryFilesView() {}
DirectoryFilesView::~DirectoryFilesView() {}

Window::Window(NavBar &nav_bar, CurrentDirectoryBar &directory_bar,
               DirectoryFilesView &directory_view, FileSystem &file_system)
    : navigate_buttons_(&nav_bar),
      current_directory_bar_(&directory_bar),
      directory_view_(&directory_view),
      file_system_(&file_system) {
  navigate_buttons_->OnBackButtonPress([this]() { this->GoBackDirectory(); });
  navigate_buttons_->OnUpButtonPress([this]() { this->GoUpDirectory(); });
  navigate_buttons_->OnForwardButtonPress(
      [this]() { this->GoForwardDirectory(); });

  current_directory_bar_->OnFileToSearchEntered(
      [this](const Glib::ustring &file_name, [[maybe_unused]] int *) {
        this->SearchForFile(file_name);
      });
  current_directory_bar_->OnDirectoryChange(
      [this](const Glib::ustring &directory_name, [[maybe_unused]] int *) {
        this->UpdateDirectory(directory_name);
      });

  directory_view_->OnFileClick([this](const Glib::ustring &file_name) {
    this->ShowFileDetails(file_name);
  });
  directory_view_->OnDirectoryClick(
      [this](const Glib::ustring &directory_name) {
        this->UpdateDirectory(directory_name);
      });
}

Window::~Window() {}

void Window::GoBackDirectory() {
  if (back_directory_history_.empty()) return;

  forward_directory_history_.push(current_directory_);

  current_directory_ = back_directory_history_.top();
  back_directory_history_.pop();
}

void Window::GoUpDirectory() {
  if (current_directory_ == "/") {
    return;
  }

  std::vector<std::string> split_directories =
      absl::StrSplit(current_directory_.c_str(), "/");
  // We know we have at least one directory (besides root) if we have more than
  // three splits (two forward slashes).
  if (split_directories.size() < 3) {
    return;
  }

  back_directory_history_.push(current_directory_);

  // Second to last string in split directories should be directory to remove.
  current_directory_ = RemoveLastDirectoryFromPath(
      current_directory_, *(split_directories.rbegin() + 1));

  // Any directory change not using history should clear forward history.
  forward_directory_history_ = std::stack<Glib::ustring>();
}

void Window::GoForwardDirectory() {
  if (forward_directory_history_.empty()) return;

  back_directory_history_.push(current_directory_);

  current_directory_ = forward_directory_history_.top();
  forward_directory_history_.pop();
}

dirent *Window::SearchForFile(const Glib::ustring &file_name) {
  return nullptr;
}

bool Window::UpdateDirectory(const Glib::ustring &new_directory) {
  if (!file_system_->GetDirectoryFiles(new_directory).ok()) return false;

  Glib::ustring cleaned_new_directory = new_directory;
  if (cleaned_new_directory.at(cleaned_new_directory.length() - 1) !=
      gunichar('/'))
    cleaned_new_directory += '/';

  // Don't update directory if we are already here. Keeps some logic simplified.
  if (current_directory_ == cleaned_new_directory) return false;

  back_directory_history_.push(current_directory_);
  forward_directory_history_ = std::stack<Glib::ustring>();

  current_directory_ = cleaned_new_directory;

  current_directory_bar_->SetDisplayedDirectory(new_directory);

  return true;
}

void Window::ShowFileDetails(const Glib::ustring &file_name) {}

NavBar &Window::GetNavBar() { return *navigate_buttons_.get(); }
CurrentDirectoryBar &Window::GetDirectoryBar() {
  return *current_directory_bar_.get();
}
DirectoryFilesView &Window::GetDirectoryFilesView() {
  return *directory_view_.get();
}
FileSystem &Window::GetFileSystem() { return *file_system_.get(); }
Glib::ustring Window::GetCurrentDirectory() { return current_directory_; }

bool CurrentDirectoryBar::SetDisplayedDirectory(
    const Glib::ustring &new_directory) {
  return true;
}

UIWindow::UIWindow()
    : ::Window(*new UINavBar(), *new UICurrentDirectoryBar(),
               *new UIDirectoryFilesView(), *new POSIXFileSystem()) {
  add(window_widgets_);

  set_default_size(600, 600);

  // Insert the navigation bar at the top left of the window. Have to dynamic
  // cast it since it's stored as the base type, but we know it's for sure a
  // UINavBar, so no error-checking.
  auto &nav_bar = dynamic_cast<UINavBar &>(GetNavBar());
  window_widgets_.attach(nav_bar.GetBorder(), /*left=*/0, /*top=*/0);

  auto &directory_bar =
      dynamic_cast<UICurrentDirectoryBar &>(GetDirectoryBar());
  window_widgets_.attach(directory_bar.GetBorder(), /*left=*/1, /*top=*/0);

  auto &directory_files_view =
      dynamic_cast<UIDirectoryFilesView &>(GetDirectoryFilesView());
  window_widgets_.attach(directory_files_view.GetWindow(), /*left=*/1,
                         /*top=*/1);

  show_all();
}
