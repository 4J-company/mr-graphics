![mr. Graphics](./mr-graphics-logo.png)

# Model Renderer Graphics Library

A high-performance cross-platform rendering library for modern graphics engines.

## Building

### Prerequisites

Install the Vulkan SDK.

Install `conan` package manager using `pip`:
```bash
pip install conan
```

Detect and configure your conan profile. Make sure to verify the compiler version and C++ standard (we use C++23). You can update the configuration file located at `$HOME/.conan2/profiles/default`:
```bash
conan profile detect
```

Set up the global CPM source cache directory (create it if it doesn't exist):
```bash
$CPM_SOURCE_CACHE="$HOME\.cache\CPM"        # Windows
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"  # Linux/macOS
```

### Configure Conan Repository for Model Renderer Modules

Clone the conan center index repository to any directory on your system:
```bash
git clone https://github.com/4J-company/conan-center-index
conan remote add 4J-company ./conan-center-index --type local-recipes-index
```

**Note:** If conan fails with `ERROR: Package 'mr-*/?.?.?' not resolved`, pull the latest changes from the master branch of the `conan-center-index` repository.

### Clone the Project

```bash
git clone https://github.com/4J-company/mr-graphics.git
cd mr-graphics
```

### Build the Project with Conan

```bash
conan build -b missing .
```

By default, this builds in Release mode. To build in Debug mode, append `-s build_type=Debug` to the build command.

## Running Examples

Extract your 3D models to `bin/models` (this is the default search path, though absolute paths are also supported). Currently, only the glTF format is supported.

Example executable files can be found in `build/Release/examples`:

- **`mr-graphics-example`**: Renders 3D models and includes a CLI. Run with `--help` to see available options.

- **`mr-simple-bench`**: Creates multiple instances of models for benchmarking purposes. Use `--bench-instances-number=$N` to specify the number of instances. Additional options:
  - `--disable-occlusion-culling`: Disables two-phase occlusion culling
  - `--disable-culling`: Disables both frustum culling and two-phase occlusion culling