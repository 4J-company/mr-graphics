#include <system.hpp>

void xmain(int argc, const char **argv)
{
	ter::application App;

	auto a = std::make_unique<window_system::window>(800, 600);
	auto b = std::make_unique<ter::window_context>(a.get(), App);
}
