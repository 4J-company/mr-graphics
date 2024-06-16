import Renderer;

int main(int argc, const char **argv)
{
  mr::Application app;

  auto wnd = app.create_window(800, 600);

  // TMP theme
  while (true) { wnd->render(); }
}
