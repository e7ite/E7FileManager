#ifndef GUI_HPP
#define GUI_HPP

#include <functional>
#include <memory>

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

// Base data structure for application window that holds all internal state,
// and avoids containing any information related to GTK, tests, etc. Add
// external data through inheritance.
class Window {
 public:
  // Dependancy injection method that will take ownership of the nav_bar object.
  Window(NavBar *nav_bar);

  Window(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(const Window &) = delete;
  Window &operator=(Window &&) = delete;
  virtual ~Window();

  virtual void GoBackDirectory();
  virtual void GoForwardDirectory();
  virtual void GoUpDirectory();

  // Can be used to extract a non-owning handle to the navigation bar structure.
  NavBar &GetNavBar();

 private:
  Window(NavBar &nav_bar);

  std::unique_ptr<NavBar> navigate_buttons_;
};

#endif  // GUI_HPP
