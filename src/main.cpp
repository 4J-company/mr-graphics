#include <system.hpp>

/**
 * @brief Program entry point.
 *
 * Initializes the application by creating a window with dimensions 640x480 and starting its main loop.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return int Exit status of the program.
 */
int main(int argc, const char **argv)
{
  mr::Application app;

  auto wnd = app.create_window({640, 480});
  wnd->start_main_loop();
}
