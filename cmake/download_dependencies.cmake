CPMAddPackage(
  NAME CrossWindow
  GITHUB_REPOSITORY cone-forest/CrossWindow
  GIT_TAG master
  )
CPMAddPackage(
  NAME CrossWindowGraphics
  GITHUB_REPOSITORY cone-forest/CrossWindow-Graphics
  GIT_TAG master
  )
CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY gracicot/stb-cmake
  GIT_TAG master
  )

set(stb_ROOT ${stb_SOURCE_DIR})
