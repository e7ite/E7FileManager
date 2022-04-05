#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>

#include "gui.hpp"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app =
      Gtk::Application::create(argc, argv, "org.gtkmm.examples.base");

  UIWindow window;
  // Unfortunately needed because this cannot be invoked in the window
  // constructor due to needing to be virtual for testing purposes.
  window.RefreshWindowComponents();

  return app->run(window);
}
