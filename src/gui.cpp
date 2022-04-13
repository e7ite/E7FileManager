#include "gui.hpp"

#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <dirent.h>
#include <gdk/gdkpixbuf.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <stack>

namespace {

Glib::ustring RemoveLastDirectoryFromPath(const Glib::ustring &full_path,
                                          const Glib::ustring &last_path);

// Verifies the entered directory with the file system. Returns false if the
// directory entered is not found.
//
// Only full paths are accepted through new_directory. Relative paths are not.
absl::StatusOr<Glib::ustring> VerifyAndCleanDirectoryUpdate(
    const Glib::ustring &old_directory, const Glib::ustring &new_directory,
    const FileSystem &fs);

// Creates an image with automatic memory management, scale it to the specified
// width and height .
Gtk::Image *CreateManagedImage(const std::string &image_path, int width,
                               int height, Gdk::PixbufRotation rotation_angle);

class UINavBar : public NavBar {
 public:
  UINavBar() {
    // Allows buttons in border to remain in top left corner.
    border_.set_halign(Gtk::ALIGN_START);
    border_.set_valign(Gtk::ALIGN_START);

    back_button_.set_image(
        *CreateManagedImage("/project/icons/arrow.png", 16, 16,
                            Gdk::PixbufRotation::PIXBUF_ROTATE_NONE));
    back_button_.set_always_show_image(true);
    forward_button_.set_image(
        *CreateManagedImage("/project/icons/arrow.png", 16, 16,
                            Gdk::PixbufRotation::PIXBUF_ROTATE_UPSIDEDOWN));
    forward_button_.set_always_show_image(true);
    up_button_.set_image(
        *CreateManagedImage("/project/icons/arrow.png", 16, 16,
                            Gdk::PixbufRotation::PIXBUF_ROTATE_CLOCKWISE));
    up_button_.set_always_show_image(true);

    // Insert in this order so the up button is at the right, and the back
    // button is at the left.
    border_.pack_start(back_button_, Gtk::PackOptions::PACK_SHRINK);
    border_.pack_start(forward_button_, Gtk::PackOptions::PACK_SHRINK);
    border_.pack_start(up_button_, Gtk::PackOptions::PACK_SHRINK);

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

  Glib::ustring GetDirectoryBarText() override {
    return this->current_directory_entry_box_.get_text();
  }
  Glib::ustring GetFileSearchBarText() override {
    return this->file_search_entry_box_.get_text();
  }

  void OnDirectoryChange(std::function<void()> callback) override {
    current_directory_entry_box_.signal_activate().connect(callback);
  }

  void OnFileToSearchEntered(std::function<void()> callback) override {
    file_search_entry_box_.signal_activate().connect(callback);
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
      std::function<void(const Glib::ustring &)> callback) override {
    file_clicked_callback_ = callback;
  }
  void OnDirectoryClick(
      std::function<void(const Glib::ustring &)> callback) override {
    directory_clicked_callback_ = callback;
  }

  void AddFile(const File &file) override {
    std::string icon_full_path = file.IsDirectory()
                                     ? "/project/icons/folder.png"
                                     : "/project/icons/empty.png";
    auto *button = Gtk::make_managed<Gtk::ToggleButton>(file.GetName());
    button->set_hexpand(true);
    button->set_image(*CreateManagedImage(
        icon_full_path, 16, 16, Gdk::PixbufRotation::PIXBUF_ROTATE_NONE));
    button->set_always_show_image(true);
    button->set_image_position(Gtk::PositionType::POS_LEFT);
    button->set_alignment(0.0f, 0.5f);

    button->signal_button_press_event().connect(
        [this, file](GdkEventButton *button_event) -> bool {
          this->file_clicked_callback_(file.GetName());
          return true;
        });
    file_entry_widgets_.pack_start(*button);
  }

  void RemoveAllFiles() override {
    for (Gtk::Widget *file_entry : file_entry_widgets_.get_children()) {
      file_entry_widgets_.remove(*file_entry);
      delete file_entry;
    }
  }

  Gtk::ScrolledWindow &GetWindow() { return file_entries_window_; }

 private:
  std::function<void(const Glib::ustring &)> file_clicked_callback_;
  std::function<void(const Glib::ustring &)> directory_clicked_callback_;
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

// Verifies the entered directory with the file system. Returns false if the
// directory entered is not found.
//
// Only full paths are accepted through new_directory. Relative paths are not.
absl::StatusOr<Glib::ustring> VerifyAndCleanDirectoryUpdate(
    const Glib::ustring &old_directory, const Glib::ustring &new_directory,
    const FileSystem &fs) {
  if (!fs.GetDirectoryFiles(new_directory).ok())
    return absl::NotFoundError("Directory not found!");

  Glib::ustring cleaned_new_directory = new_directory;
  if (cleaned_new_directory.at(cleaned_new_directory.length() - 1) !=
      gunichar('/'))
    cleaned_new_directory += '/';

  // Don't update directory if we are already here. Keeps some logic simplified.
  if (old_directory == cleaned_new_directory)
    return absl::InvalidArgumentError("Already in this directory");

  return cleaned_new_directory;
}

// Creates an image with automatic memory management, scale it to the specified
// width and height .
Gtk::Image *CreateManagedImage(const std::string &image_path, int width,
                               int height, Gdk::PixbufRotation rotation_angle) {
  Glib::RefPtr<Gdk::Pixbuf> image_buf;
  try {
    image_buf = Gdk::Pixbuf::create_from_file(image_path, width, height);
  } catch (const Glib::FileError &file_error) {
    std::cerr << "Caught Glib::FileError: " << std::string(file_error.what())
              << std::endl;
    return nullptr;
  } catch (const Gdk::PixbufError &pixbuf_error) {
    std::cerr << "Caught Gdk::PixbufError: " << std::string(pixbuf_error.what())
              << std::endl;
    return nullptr;
  }

  if (rotation_angle != Gdk::PixbufRotation::PIXBUF_ROTATE_NONE) {
    Glib::RefPtr<Gdk::Pixbuf> rotated_image_buf =
        image_buf->rotate_simple(rotation_angle);
    if (rotated_image_buf)
      image_buf = rotated_image_buf;
    else
      std::cerr << "Failed to rotate " << image_path
                << ". Returning non-rotated image\n";
  }

  return Gtk::make_managed<Gtk::Image>(image_buf);
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
  navigate_buttons_->OnBackButtonPress([this]() {
    this->GoBackDirectory();
    this->RefreshWindowComponents();
  });
  navigate_buttons_->OnUpButtonPress([this]() {
    this->GoUpDirectory();
    this->RefreshWindowComponents();
  });
  navigate_buttons_->OnForwardButtonPress([this]() {
    this->GoForwardDirectory();
    this->RefreshWindowComponents();
  });

  current_directory_bar_->OnFileToSearchEntered([this]() {
    this->SearchForFile(this->GetDirectoryBar().GetFileSearchBarText());
  });
  current_directory_bar_->OnDirectoryChange([this]() {
    this->HandleFullDirectoryChange(
        this->GetDirectoryBar().GetDirectoryBarText());
    this->RefreshWindowComponents();
  });

  directory_view_->OnFileClick([this](const Glib::ustring &file_name) {
    // Assumes the file name passed is relative without any directory notation
    // on it.
    std::string new_directory = this->GetCurrentDirectory() + file_name;
    this->HandleFullDirectoryChange(new_directory);
    this->RefreshWindowComponents();
  });
  directory_view_->OnDirectoryClick([this](
                                        const Glib::ustring &directory_name) {
    // Assumes the directory name passed is relative without any directory
    // notation on it.
    std::string new_directory = this->GetCurrentDirectory() + directory_name;
    this->HandleFullDirectoryChange(new_directory);
    this->RefreshWindowComponents();
  });
}

Window::~Window() {}

void Window::GoBackDirectory() {
  if (back_directory_history_.empty()) return;

  std::string previous_directory = back_directory_history_.top();

  forward_directory_history_.push(current_directory_);
  back_directory_history_.pop();

  UpdateDirectory(previous_directory);
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

  // Any directory change not using history should clear forward history.
  back_directory_history_.push(current_directory_);
  forward_directory_history_ = std::stack<Glib::ustring>();

  // Second to last string in split directories should be directory to remove.
  std::string updated_path = RemoveLastDirectoryFromPath(
      current_directory_, *(split_directories.rbegin() + 1));

  UpdateDirectory(updated_path);
}

void Window::GoForwardDirectory() {
  if (forward_directory_history_.empty()) return;

  std::string previous_directory = forward_directory_history_.top();

  back_directory_history_.push(current_directory_);
  forward_directory_history_.pop();

  UpdateDirectory(previous_directory);
}

dirent *Window::SearchForFile(const Glib::ustring &file_name) {
  return nullptr;
}

void Window::UpdateDirectory(const Glib::ustring &new_directory) {
  current_directory_ = new_directory;
}

void Window::HandleFullDirectoryChange(const Glib::ustring &new_directory) {
  absl::StatusOr<Glib::ustring> new_cleaned_directory =
      VerifyAndCleanDirectoryUpdate(current_directory_, new_directory,
                                    *file_system_);
  if (!new_cleaned_directory.ok()) return;

  back_directory_history_.push(current_directory_);
  forward_directory_history_ = std::stack<Glib::ustring>();

  UpdateDirectory(new_cleaned_directory.value());
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
}

void UIWindow::RefreshWindowComponents() {
  const Glib::ustring &new_directory = GetCurrentDirectory();

  absl::StatusOr<std::vector<File>> file_names =
      GetFileSystem().GetDirectoryFiles(new_directory);
  if (!file_names.ok()) return;

  GetDirectoryFilesView().RemoveAllFiles();

  for (const File &file : file_names.value()) {
    GetDirectoryFilesView().AddFile(file);
  }

  GetDirectoryBar().SetDisplayedDirectory(new_directory);

  show_all();
}
