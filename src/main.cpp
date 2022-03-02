#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>

#include "gui.hpp"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app =
      Gtk::Application::create(argc, argv, "org.gtkmm.examples.base");

  UIWindow window;

  return app->run(window);
}
