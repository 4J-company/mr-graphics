#include <system.hpp>

int main(int argc, const char **argv)
{
  mr::Application app;

  auto wnd = app.create_window({800, 600});
  wnd->start_main_loop();
}
