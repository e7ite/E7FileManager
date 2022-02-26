#ifndef GUI_HPP
#define GUI_HPP

#include <dirent.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <string_view>

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

// Used to forward signal when text is entered to search for a file.
class FileSearchBar {
 public:
  FileSearchBar();
  FileSearchBar(const FileSearchBar &) = delete;
  FileSearchBar(FileSearchBar &&) = delete;
  FileSearchBar &operator=(const FileSearchBar &) = delete;
  FileSearchBar &operator=(FileSearchBar &&) = delete;
  virtual ~FileSearchBar();

  virtual void OnFileToSearchEntered(
      std::function<void(const Glib::ustring &, int *)> callback) = 0;
};

// Used to request a change to the current directory, and to receive a signal
// from the main window that the directory has been changed by the window
// itself.
class CurrentDirectoryBar {
 public:
  CurrentDirectoryBar();

  CurrentDirectoryBar(const CurrentDirectoryBar &) = delete;
  CurrentDirectoryBar(CurrentDirectoryBar &&) = delete;
  CurrentDirectoryBar &operator=(const CurrentDirectoryBar &) = delete;
  CurrentDirectoryBar &operator=(CurrentDirectoryBar &&) = delete;
  virtual ~CurrentDirectoryBar();

  // This can be used to register an action to take when there is a request to
  // update the directory, specifically from the directory bar. This can be
  // such as when a user manually enters a directory to navigate to.
  virtual void OnDirectoryChange(
      std::function<void(const Glib::ustring &, int *)> callback) = 0;
};

// Base data structure for application window that holds all internal state,
// and avoids containing any information related to GTK, tests, etc. Add
// external data through inheritance.
class Window {
 public:
  // Dependancy injection method that will take ownership of the nav_bar object.
  Window(NavBar *nav_bar, FileSearchBar *search_bar,
         CurrentDirectoryBar *directory_bar);

  Window(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(const Window &) = delete;
  Window &operator=(Window &&) = delete;
  virtual ~Window();

  virtual void GoBackDirectory();
  virtual void GoForwardDirectory();
  virtual void GoUpDirectory();

  // Method to request for the window to change the current directory, based on
  // the argument. Returns false if the directory entered is not found.
  //
  // Only full paths are accepted through new_directory. Relative paths are not.
  virtual bool UpdateDirectory(Glib::UStringView new_directory);

  virtual dirent *SearchForFile(Glib::UStringView file_name);

  // Can be used to extract non-owning handles to GUI internal widgets.
  NavBar &GetNavBar();
  FileSearchBar &GetFileSearchBar();
  CurrentDirectoryBar &GetDirectoryBar();

 private:
  Window(NavBar &nav_bar, FileSearchBar &search_bar,
         CurrentDirectoryBar &directory_bar);

  std::unique_ptr<NavBar> navigate_buttons_;
  std::unique_ptr<FileSearchBar> file_search_bar_;
  std::unique_ptr<CurrentDirectoryBar> current_directory_bar_;
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

 private:
  UIWindow(NavBar &nav_bar);

  Gtk::Box window_widgets_;
};

#endif  // GUI_HPP
