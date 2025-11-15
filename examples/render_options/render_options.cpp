#include "render_options.hpp"

#include <boost/program_options.hpp>

namespace po = boost::program_options;

static std::optional<std::pair<uint32_t, uint32_t>> parse_resolution(const std::string &s)
{
  uint32_t width, height;
  if (sscanf(s.c_str(), "%dx%d", &width, &height) != 2) {
    return std::nullopt;
  }
  return std::make_pair(width, height);
}

static std::optional<mr::math::Camera<float>> parse_camera(const std::string &s)
{
  float data[3][3];
  if (sscanf(s.c_str(), "( (%f,%f,%f) , (%f,%f,%f) , (%f,%f,%f) )",
        &data[0][0], &data[0][1], &data[0][2],
        &data[1][0], &data[1][1], &data[1][2],
        &data[2][0], &data[2][1], &data[2][2]) != 9) {
    return std::nullopt;
  }
  return mr::math::Camera<float>(
    mr::Vec3f(data[0][0], data[0][1], data[0][2]),
    mr::Vec3f(data[1][0], data[1][1], data[1][2]).normalize(),
    mr::Vec3f(data[2][0], data[2][1], data[2][2]).normalize()
  );
}

static std::optional<mr::RenderOptions::Mode> parse_mode(std::string_view s)
{
  if (s == "default") {
    return mr::RenderOptions::Mode::Default;
  } else if (s == "frames") {
    return mr::RenderOptions::Mode::Frames;
  } else if (s == "bench") {
    return mr::RenderOptions::Mode::Bench;
  } else {
    return std::nullopt;
  }
}

std::optional<mr::RenderOptions> mr::RenderOptions::parse(int argc, const char **argv)
{
  po::options_description desc("mr-cli - Model Renderer CLI");
  // TODO(dk6): maybe add ability to specify default values outside instead hardcode here
  desc.add_options()
    ("help,h", "Show help message")
    ("mode",
     po::value<std::string>()->default_value("default"),
     "Mode of model renderer ('default', 'frames', 'bench'). By default 'default')")
    ("dst-dir",
     po::value<std::string>()->default_value("frames"),
     "Destination directory for frames (default: ./frames). Used only for frames mode")
    ("frames-number",
     po::value<int>()->default_value(1),
     "Number of frames to render (default: 1)")
    ("resolution",
     po::value<std::string>()->default_value("1920x1080"),
     "Resolution in format WIDTHxHEIGHT (default: 1920x1080)")
    ("camera",
     po::value<std::string>(),
     "Camera parameters as ((pos_x,pos_y,pos_z), (target_x,target_y,target_z), (up_x,up_y,up_z))")
    ("bench-name",
     po::value<std::string>(),
     "Name of benchmark")
    ("disable-culling",
     po::bool_switch()->default_value(false),
     "Disable culling")
    ("enable-bound-boxes",
     po::bool_switch()->default_value(false),
     "Enable drawing bound boxes of models for debug")
    ("stat-dir",
     po::value<std::string>()->default_value("render_stats"),
     "Path to directory in which frame stats will be writed (default: ./render_stat.txt).")
    ("models",
     po::value<std::vector<std::string>>()->multitoken(),
     "GLTF model files to render (can be specified anywhere in arguments)")
  ;

  po::positional_options_description pos_desc;
  pos_desc.add("models", -1);

  po::variables_map vm;

  try {
    po::store(po::command_line_parser(argc, argv)
              .options(desc)
              .positional(pos_desc)
              .run(),
              vm);
  } catch (...) {
    std::println(std::cerr, "Error: Failed to parse command line arguments");
    return std::nullopt;
  }

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    std::println("\nExamples:");
    std::println("  mr-cli model.gltf --dst-dir=./output --frames-number=10");
    std::println("  mr-cli --mode=bench --resolution=3840x2160 model1.gltf model2.gltf");
    std::println("  mr-cli --models scene.gltf --camera=\"((1,2,3),(0,0,0),(0,1,0))\" --disable-culling");
    std::println("  mr-cli --mode=default --resolution=100x100 /path/to/model1 /path/to/model2");
    return std::nullopt;
  }

  try {
    po::notify(vm);
  } catch (...) {
    std::println(std::cerr, "Error: Missing required arguments");
    std::println(std::cerr, "Use --help for usage information");
    return std::nullopt;
  }

  RenderOptions options;

  options.dst_dir = vm["dst-dir"].as<std::string>();
  options.frames_number = vm["frames-number"].as<int>();
  options.disable_culling = vm["disable-culling"].as<bool>();
  options.enable_bound_boxes = vm["enable-bound-boxes"].as<bool>();
  options.stat_dir = vm["stat-dir"].as<std::string>();

  auto mode_str = vm["mode"].as<std::string>();
  auto mode_opt = parse_mode(mode_str);
  if (not mode_opt) {
    std::println(std::cerr, "Error: Invalid mode : {}", mode_str);
    std::println(std::cerr, "Expected values: 'default', 'frames', 'bench'");
    return std::nullopt;
  }
  options.mode = *mode_opt;

  std::string res_str = vm["resolution"].as<std::string>();
  auto resolution_opt = parse_resolution(res_str);
  if (!resolution_opt) {
    std::println(std::cerr, "Error: Invalid resolution format: {}", res_str);
    std::println(std::cerr, "Expected format: WIDTHxHEIGHT (e.g., 1920x1080)");
    return std::nullopt;
  }
  std::tie(options.width, options.height) = *resolution_opt;

  if (vm.count("camera")) {
    std::string cam_str = vm["camera"].as<std::string>();
    auto camera_opt = parse_camera(cam_str);
    if (!camera_opt) {
      std::println(std::cerr, "Error: Invalid camera format: {}", cam_str);
      std::println(std::cerr, "Expected format: ((x,y,z),(x,y,z),(x,y,z))");
      return std::nullopt;
    }
    options.camera = *camera_opt;
  }

  if (vm.count("bench-name")) {
    options.bench_name = vm["bench-name"].as<std::string>();
  }

  if (vm.count("models")) {
    options.models = vm["models"].as<std::vector<std::string>>() |
      std::views::transform([](const auto &s){return std::fs::path(s);}) | std::ranges::to<std::vector>();
  }

  if (options.models.empty()) {
    std::println(std::cerr, "Error: No model files specified");
    std::println(std::cerr, "Use --help for usage information");
    return std::nullopt;
  }

  return options;
}

void mr::RenderOptions::print() const noexcept
{
  std::println("=== Configuration ===");

  std::println("mode: {}", mode == Mode::Default ? "default" :
                           mode == Mode::Frames ? "frames" :
                           mode == Mode::Bench ? "bench"
                           : "invalid");

  if (mode == Mode::Frames) {
    std::println("Destination directory: {}", dst_dir.c_str());
  }
  if (mode != Mode::Default) {
    std::println("Frames number: {}", frames_number);
  }
  std::println("Resolution: {}x{}", width, height);
  std::println("Culling: {}", disable_culling ? "DISABLED" : "ENABLED");
  std::println("Statistics directory: {}", stat_dir.c_str());

  if (camera) {
    std::println("camera:");
    std::cout
      << "    " << camera->position() << std::endl
      << "    " << camera->direction() << std::endl
      << "    " << camera->up() << std::endl;
  }

  std::println("Model files ({}):", models.size());
  for (const auto& path : models) {
    std::println("  - {}", path.c_str());
  }
}

