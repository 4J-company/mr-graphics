#include <system.hpp>

void xmain(int argc, const char **argv)
{
  mr::Application App;

  auto wnd = App.create_window(800, 600);

  // TMP theme
  while (true)
    wnd->render();
}
