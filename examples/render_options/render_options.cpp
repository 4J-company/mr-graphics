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
    mr::Norm3f(data[1][0], data[1][1], data[1][2]),
    mr::Norm3f(data[2][0], data[2][1], data[2][2]));
}

static std::optional<mr::math::Camera<float>::Projection> parse_projection(const std::string &s)
{
  float near, far;
  if (sscanf(s.c_str(), "[ %f , %f ]", &near, &far) != 2) {
    return std::nullopt;
  }
  return mr::math::Camera<float>::Projection(45_deg, near, far);
}

static std::optional<mr::Extent> parse_msoc_tile_size(const std::string &s)
{
  uint32_t width, height;
  if (sscanf(s.c_str(), "[ %u , %u ]", &width, &height) != 2) {
    return std::nullopt;
  }
  return mr::Extent{width, height};
}

static std::optional<mr::CliOptions::Mode> parse_mode(std::string_view s)
{
  if (s == "default") {
    return mr::CliOptions::Mode::Default;
  } else if (s == "frames") {
    return mr::CliOptions::Mode::Frames;
  } else if (s == "bench") {
    return mr::CliOptions::Mode::Bench;
  } else {
    return std::nullopt;
  }
}

std::optional<mr::CliOptions> mr::CliOptions::parse(int argc, const char **argv)
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
     po::value<int>(),
     "Number of frames to render (if not set it became infinity)")
    ("resolution",
     po::value<std::string>()->default_value("1920x1080"),
     "Resolution in format WIDTHxHEIGHT (default: 1920x1080)")
    ("camera",
     po::value<std::string>(),
     "Camera parameters as '((pos_x,pos_y,pos_z), (dir_x, dir_y, dir_z), (up_x,up_y,up_z))'")
    ("proj",
     po::value<std::string>(),
     "Camera projection in format '[near, far]'")
    ("bench-name",
     po::value<std::string>(),
     "Name of benchmark")
    ("bench-models-number",
     po::value<uint32_t>(),
     "Number of models to render for benchmarking")
    ("disable-culling",
     po::bool_switch()->default_value(false),
     "Disable culling")
    ("disable-occlusion-culling",
     po::bool_switch()->default_value(false),
     "Disable occlusion culling")
    ("enable-vsync",
     po::bool_switch()->default_value(false),
     "Enable VSYNC")
    ("enable-bound-boxes",
     po::bool_switch()->default_value(false),
     "Enable drawing bound boxes of models for debug")
    ("stat-dir",
     po::value<std::string>()->default_value("render_stats"),
     "Path to directory in which frame stats will be written (default: render_stats).")
    ("print-stat",
     po::bool_switch()->default_value(false),
     "Print stat in cout")
    ("bench-instances-number",
     po::value<uint32_t>()->default_value(1'000),
     "Models instances number for bench")
    ("models",
     po::value<std::vector<std::string>>()->multitoken(),
     "GLTF model files to render (can be specified anywhere in arguments)")
    ("enable-culling-stat",
     po::bool_switch()->default_value(false),
     "Collect statistics of culling")
    ("enable-culling-visualization",
     po::bool_switch()->default_value(false),
     "Stash invisible objects on '0' key")
    ("read-gbuf",
     po::bool_switch()->default_value(false),
     "Read first gbuffer with positions and instance ids")
    ("hash-coloring",
     po::bool_switch()->default_value(false),
     "Use hash coloring for objects")
    ("render-bounds-state",
     po::value<std::string>(),
     "Render bounds state, can be: Disable, BoundBoxes, BoundRectangles")
    ("oc-bounds",
     po::value<std::string>(),
     "Occlusion culling bounds: Box, Sphere, DynamicBest, Both")
    ("oc-type",
     po::value<std::string>(),
     "Occlusion culling type: HiZ, MSOC, msoc-adaptive-tile-size")
    ("msoc-with-hiz-coarse",
     po::bool_switch()->default_value(false),
     "Enable coarse HiZ prep pass before MSOC tile tests")
    ("msoc-tile-size",
     po::value<std::string>()->default_value("[8, 8]"),
     "MSOC tile size in format '[tilew, tileh]' (default: [8, 8])")
    ("msoc-tiles-per-thread",
     po::value<uint32_t>()->default_value(8),
     "MSOC tiles processed per thread (default: 8)")
    ("oc-draw-always",
     po::bool_switch()->default_value(false),
     "Run occlusion culling but still draw culled instances (debug)")
    ("save-gbuffer-frame",
     po::value<uint32_t>(),
     "1-based frame index to dump Position gbuffer (.gbuf)")
    ("save-gbuffer-image-path",
     po::value<std::string>(),
     "Output path for gbuffer dump (use with --save-gbuffer-frame)")
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

  CliOptions options;

  options.dst_dir = vm["dst-dir"].as<std::string>();
  options.disable_culling = vm["disable-culling"].as<bool>();
  options.disable_occlusion_culling = vm["disable-occlusion-culling"].as<bool>();
  options.enable_vsync = vm["enable-vsync"].as<bool>();
  options.enable_bound_boxes = vm["enable-bound-boxes"].as<bool>();
  options.stat_dir = vm["stat-dir"].as<std::string>();
  options.print_stat = vm["print-stat"].as<bool>();
  options.enable_culling_stat = vm["enable-culling-stat"].as<bool>();
  options.enable_culling_visualization = vm["enable-culling-visualization"].as<bool>();
  options.read_gbuf = vm["read-gbuf"].as<bool>();
  options.hash_coloring = vm["hash-coloring"].as<bool>();
  options.msoc_with_hiz_coarse = vm["msoc-with-hiz-coarse"].as<bool>();
  options.oc_draw_always = vm["oc-draw-always"].as<bool>();

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

  if (vm.count("proj")) {
    std::string proj_str = vm["proj"].as<std::string>();
    auto proj_opt = parse_projection(proj_str);
    if (!proj_opt) {
      std::println(std::cerr, "Error: Invalid camera format: {}", proj_str);
      std::println(std::cerr, "Expected format: '[near, far]'");
      return std::nullopt;
    }
    options.projection = *proj_opt;
  }

  if (vm.count("bench-name")) {
    options.bench_name = vm["bench-name"].as<std::string>();
  }

  if (vm.count("bench-instances-number")) {
    options.bench_instances_number = vm["bench-instances-number"].as<uint32_t>();
  }

  if (vm.count("models")) {
    options.models = vm["models"].as<std::vector<std::string>>() |
      std::views::transform([](const auto &s){return std::fs::path(s);}) | std::ranges::to<std::vector>();
  }

  if (vm.count("frames-number")) {
    options.frames_number = vm["frames-number"].as<int>();
  }

  if (vm.count("render-bounds-state")) {
    std::flat_map<std::string_view, mr::graphics::RenderBoundsState> bounds_states {
      {"Disable", mr::graphics::RenderBoundsState::Disable},
      {"BoundBoxes", mr::graphics::RenderBoundsState::BoundBoxes},
      {"BoundBoxRectangles", mr::graphics::RenderBoundsState::BoundBoxRectangles},
      {"BoundSpheres", mr::graphics::RenderBoundsState::BoundSpheres},
      {"BoundSphereRectangles", mr::graphics::RenderBoundsState::BoundSphereRectangles},
      {"DynamicBest", mr::graphics::RenderBoundsState::DynamicBest},
      {"DynamicBestRectangles", mr::graphics::RenderBoundsState::DynamicBestRectangles},
    };
    auto value = vm["render-bounds-state"].as<std::string>();
    auto it = bounds_states.find(value);
    if (it != bounds_states.end()) {
      options.bounds_state = it->second;
    } else {
      MR_ERROR("Invalid 'render-bounds-state' value: '{}'", value);
    }
  }

  if (vm.count("oc-bounds")) {
    std::flat_map<std::string_view, mr::graphics::OcclusionCullingBounds> bounds_states {
      {"Box", mr::graphics::OcclusionCullingBounds::Box},
      {"Sphere", mr::graphics::OcclusionCullingBounds::Sphere},
      {"DynamicBest", mr::graphics::OcclusionCullingBounds::DynamicBest},
      {"Both", mr::graphics::OcclusionCullingBounds::Both},
    };
    auto value = vm["oc-bounds"].as<std::string>();
    auto it = bounds_states.find(value);
    if (it != bounds_states.end()) {
      options.oc_bounds = it->second;
    } else {
      MR_ERROR("Invalid 'render-bounds-state' value: '{}'", value);
    }
  }

  if (vm.count("oc-type")) {
    std::flat_map<std::string_view, mr::graphics::OcclusionCullingType> oc_types {
      {"HiZ", mr::graphics::OcclusionCullingType::TwoPhaseHiZ},
      {"MSOC", mr::graphics::OcclusionCullingType::MaskedSoftware},
      {"MsocAdaptiveTileSize", mr::graphics::OcclusionCullingType::MsocAdaptiveTileSize},
      {"hiz", mr::graphics::OcclusionCullingType::TwoPhaseHiZ},
      {"msoc", mr::graphics::OcclusionCullingType::MaskedSoftware},
      {"msoc-adaptive-tile-size", mr::graphics::OcclusionCullingType::MsocAdaptiveTileSize},
      {"msoc-adaptive", mr::graphics::OcclusionCullingType::MsocAdaptiveTileSize},
      // Deprecated aliases
      {"MsocHizHybrid", mr::graphics::OcclusionCullingType::MsocAdaptiveTileSize},
      {"msoc-hiz-hybrid", mr::graphics::OcclusionCullingType::MsocAdaptiveTileSize},
    };
    auto value = vm["oc-type"].as<std::string>();
    auto it = oc_types.find(value);
    if (it != oc_types.end()) {
      options.oc_type = it->second;
    } else {
      MR_ERROR("Invalid 'oc-type' value: '{}'", value);
    }
  }

  {
    std::string tile_size_str = vm["msoc-tile-size"].as<std::string>();
    auto tile_size_opt = parse_msoc_tile_size(tile_size_str);
    if (!tile_size_opt) {
      std::println(std::cerr, "Error: Invalid msoc-tile-size format: {}", tile_size_str);
      std::println(std::cerr, "Expected format: '[tilew, tileh]' (e.g., [8, 8])");
      return std::nullopt;
    }
    options.msoc_tile_size = *tile_size_opt;
  }

  options.msoc_tiles_per_thread = vm["msoc-tiles-per-thread"].as<uint32_t>();

  const bool has_save_frame = vm.count("save-gbuffer-frame") > 0;
  const bool has_save_path = vm.count("save-gbuffer-image-path") > 0;
  if (has_save_frame != has_save_path) {
    std::println(std::cerr, "Error: --save-gbuffer-frame and --save-gbuffer-image-path must be set together");
    return std::nullopt;
  }
  if (has_save_frame) {
    options.save_gbuffer_frame = vm["save-gbuffer-frame"].as<uint32_t>();
    options.save_gbuffer_image_path = vm["save-gbuffer-image-path"].as<std::string>();
    if (options.save_gbuffer_frame.value() == 0) {
      std::println(std::cerr, "Error: --save-gbuffer-frame must be >= 1");
      return std::nullopt;
    }
  }

  if (options.models.empty()) {
    std::println(std::cerr, "Error: No model files specified");
    std::println(std::cerr, "Use --help for usage information");
    return std::nullopt;
  }

  return options;
}

void mr::CliOptions::print() const noexcept
{
  std::println("=== Configuration ===");

  std::println("mode: {}", mode == Mode::Default ? "default" :
                           mode == Mode::Frames ? "frames" :
                           mode == Mode::Bench ? "bench"
                           : "invalid");

  if (mode == Mode::Frames) {
    std::println("Destination directory: {}", dst_dir.string().c_str());
  }
  if (frames_number) {
    std::println("Frames number: {}", frames_number.value());
  }
  std::println("Resolution: {}x{}", width, height);
  std::println("Culling: {}", disable_culling ? "DISABLED" : "ENABLED");
  std::println("Statistics directory: {}", stat_dir.string().c_str());

  if (camera) {
    std::println("Camera:");
    std::cout
      << "    " << camera->position() << std::endl
      << "    " << camera->direction() << std::endl
      << "    " << camera->up() << std::endl;
  }
  if (projection) {
    std::println("Projection: [{}, {}]", projection.value().distance, projection.value().far);
  }

  if (bounds_state) {
    std::println("Render bounds state: {}", enum_cast(*bounds_state));
  }

  if (oc_bounds) {
    std::println("Occlusion culling bounds: {}", enum_cast(*oc_bounds));
  }

  if (oc_type) {
    std::string_view oc_type_name = "MSOC";
    if (*oc_type == OcclusionCullingType::TwoPhaseHiZ) {
      oc_type_name = "HiZ";
    } else if (*oc_type == OcclusionCullingType::MsocAdaptiveTileSize) {
      oc_type_name = "MsocAdaptiveTileSize";
    }
    std::println("Occlusion culling type: {}", oc_type_name);
  }

  std::println("MSOC tile size: [{}, {}]", msoc_tile_size.width, msoc_tile_size.height);
  std::println("MSOC tiles per thread: {}", msoc_tiles_per_thread);
  std::println("MSOC coarse HiZ prep: {}", msoc_with_hiz_coarse ? "ENABLED" : "DISABLED");
  std::println("OC draw always: {}", oc_draw_always ? "ENABLED" : "DISABLED");

  if (save_gbuffer_frame) {
    std::println("Save gbuffer frame: {}", *save_gbuffer_frame);
    std::println("Save gbuffer path: {}", save_gbuffer_image_path->string());
  }

  std::println("Model files ({}):", models.size());
  for (const auto& path : models) {
    std::println("  - {}", path.string().c_str());
  }
}

