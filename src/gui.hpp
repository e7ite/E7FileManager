#ifndef GUI_HPP
#define GUI_HPP

#include <dirent.h>
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

// Represents the search bar data, and is used to forward signal when text is
// entered.
class SearchBar {
 public:
  SearchBar();
  SearchBar(const SearchBar &) = delete;
  SearchBar(SearchBar &&) = delete;
  SearchBar &operator=(const SearchBar &) = delete;
  SearchBar &operator=(SearchBar &&) = delete;
  virtual ~SearchBar();

  virtual void OnFileToSearchEntered(
      std::function<void(std::string_view)> callback) = 0;
};

// Base data structure for application window that holds all internal state,
// and avoids containing any information related to GTK, tests, etc. Add
// external data through inheritance.
class Window {
 public:
  // Dependancy injection method that will take ownership of the nav_bar object.
  Window(NavBar *nav_bar, SearchBar *search_bar);

  Window(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(const Window &) = delete;
  Window &operator=(Window &&) = delete;
  virtual ~Window();

  virtual void GoBackDirectory();
  virtual void GoForwardDirectory();
  virtual void GoUpDirectory();

  virtual dirent *SearchForFile(std::string_view file_name);

  // Can be used to extract non-owning handles to GUI internal widgets.
  NavBar &GetNavBar();
  SearchBar &GetSearchBar();

 private:
  Window(NavBar &nav_bar, SearchBar &search_bar);

  std::unique_ptr<NavBar> navigate_buttons_;
  std::unique_ptr<SearchBar> file_search_bar_;
};

// Main class which represents the game's window.
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
