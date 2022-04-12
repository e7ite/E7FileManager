#ifndef GUI_HPP
#define GUI_HPP

#include <dirent.h>
#include <glibmm/ustring.h>
#include <gtkmm/grid.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <stack>

#include "filesystem.hpp"

// A base interface for creating derived instances of the navigation bar,
// containing a back, forward, and up button. Can be derived to provide
// different methods to signal when the buttons are pressed.
class NavBar {
 public:
  NavBar();

  NavBar(const NavBar &) = delete;
  NavBar(NavBar &&) = delete;
  NavBar &operator=(const NavBar &) = delete;
  NavBar &operator=(NavBar &&) = delete;
  virtual ~NavBar();

  // Registers a callback to invoke when each button is pressed. If you wish to
  // pass in arguments, wrap the call in a lambda.
  virtual void OnBackButtonPress(std::function<void()> callback) = 0;
  virtual void OnForwardButtonPress(std::function<void()> callback) = 0;
  virtual void OnUpButtonPress(std::function<void()> callback) = 0;
};

// Interface for showing the current directory and searching for a file relative
// to the current directory. Used to request a change to the current directory,
// to receive a signal from the main window that the directory has been
// changed by the window itself, and to handle file search requests.
class CurrentDirectoryBar {
 public:
  CurrentDirectoryBar();

  CurrentDirectoryBar(const CurrentDirectoryBar &) = delete;
  CurrentDirectoryBar(CurrentDirectoryBar &&) = delete;
  CurrentDirectoryBar &operator=(const CurrentDirectoryBar &) = delete;
  CurrentDirectoryBar &operator=(CurrentDirectoryBar &&) = delete;
  virtual ~CurrentDirectoryBar();

  virtual Glib::ustring GetDirectoryBarText() = 0;
  virtual Glib::ustring GetFileSearchBarText() = 0;

  // This sets the internal text displayed for the current directory text box
  // (located below the file search box), to the argument. Expects the directory
  // entered to be valid.
  virtual void SetDisplayedDirectory(const Glib::ustring &new_directory) = 0;

  // This can be used to register an action to take when there is a request to
  // update the directory, specifically from the directory bar. This can be
  // such as when a user manually enters a directory to navigate to.
  virtual void OnDirectoryChange(std::function<void()> callback) = 0;

  // Registers an action to take when there is a request to search for a file.
  // Happens when the user types a file name into the file search bar.
  virtual void OnFileToSearchEntered(std::function<void()> callback) = 0;
};

// Represents the window that lists the files in a directory on the file system.
// Provides updates when a file or directory is clicked.
class DirectoryFilesView {
 public:
  DirectoryFilesView();

  DirectoryFilesView(const DirectoryFilesView &) = delete;
  DirectoryFilesView(DirectoryFilesView &&) = delete;
  DirectoryFilesView &operator=(const DirectoryFilesView &) = delete;
  DirectoryFilesView &operator=(DirectoryFilesView &&) = delete;
  virtual ~DirectoryFilesView();

  // Registers the action to take when a file is clicked in the directory view.
  // This does not include when a directory is clicked. OnDirectoryClicked() can
  // be used to check that behavior.
  virtual void OnFileClick(
      std::function<void(const Glib::ustring &)> callback) = 0;

  // Registers the action to take when a directory is clicked in the directory
  // view.
  //
  // This does not include when a file is clicked. OnFileClicked() can be used
  // to check that behavior.
  virtual void OnDirectoryClick(
      std::function<void(const Glib::ustring &)> callback) = 0;

  // Adds a file to be displayed on the file view. This can be a file or a
  // directory (following Unix's everything is a file ideology). Will register
  // the callback specified in OnFileClick() to this file.
  virtual void AddFile(const File &file) = 0;

  // Removes all files that are currently displaying in the window view.
  virtual void RemoveAllFiles() = 0;
};

// Base data structure for application window that holds all internal state,
// and avoids containing any information related to GTK, tests, etc. Add
// external data through inheritance.
class Window {
 public:
  // Dependancy injection method that will take ownership of passed in objects.
  Window(NavBar &nav_bar, CurrentDirectoryBar &directory_bar,
         DirectoryFilesView &directory_window, FileSystem &file_system);

  Window(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(const Window &) = delete;
  Window &operator=(Window &&) = delete;
  virtual ~Window();

  // Will update the current directory respective to which method below has been
  // invoked.
  //
  // TODO: Add error checking that works with GoogleTest.
  virtual void GoBackDirectory();
  virtual void GoForwardDirectory();
  virtual void GoUpDirectory();
  virtual void HandleFullDirectoryChange(const Glib::ustring &new_directory);

  // Will search for the file passed in, relative to the current directory.
  //
  // Returns nullptr if the file name does not exist.
  virtual dirent *SearchForFile(const Glib::ustring &file_name);

  // Shows window containing details of a file and a preview of it if possible.
  //
  // Does nothing if file does not exist.
  virtual void ShowFileDetails(const Glib::ustring &file_name);

  // Can be used to extract non-owning handles to GUI internal widgets.
  NavBar &GetNavBar();
  CurrentDirectoryBar &GetDirectoryBar();
  DirectoryFilesView &GetDirectoryFilesView();
  FileSystem &GetFileSystem();

  Glib::ustring GetCurrentDirectory();

  // Updates all the widgets shown on the window. Should be called after any
  // widget update such as a directory change.
  virtual void RefreshWindowComponents() = 0;

 private:
  // Asssumes new_directory to be valid.
  void UpdateDirectory(const Glib::ustring &new_directory);

  std::unique_ptr<NavBar> navigate_buttons_;
  std::unique_ptr<CurrentDirectoryBar> current_directory_bar_;
  std::unique_ptr<DirectoryFilesView> directory_view_;

  // Provides the file system the file manager can create and view files from.
  std::unique_ptr<FileSystem> file_system_;

  // Needed for handling going back and forth using the navigation bar.
  std::stack<Glib::ustring> back_directory_history_;
  std::stack<Glib::ustring> forward_directory_history_;

  Glib::ustring current_directory_ = "/";
};

// Represents the whole GUI structure including the file manager's internal
// state and all the GTK widgets required for it.
class UIWindow : public Gtk::Window, public Window {
 public:
  UIWindow();

  UIWindow(const UIWindow &) = delete;
  UIWindow(UIWindow &&) = delete;
  UIWindow &operator=(const UIWindow &) = delete;
  UIWindow &operator=(UIWindow &&) = delete;
  virtual ~UIWindow() {}

  // Updates all the window widgets after an internal update. Must be called
  // after instantiation of the window. TODO: Maybe create a better design so
  // the user doesn't have to call this themselves?
  void RefreshWindowComponents() override;

 private:
  Gtk::Grid window_widgets_;
};

#endif  // GUI_HPP
