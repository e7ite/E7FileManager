#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>

#include "gui.hpp"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app =
      Gtk::Application::create(argc, argv, "org.gtkmm.examples.base");

  UIWindow window;
  window.set_size_request(600, 600);

  return app->run(window);
}
