CPMAddPackage(
  NAME CrossWindow
  GITHUB_REPOSITORY 4J-company/mr-window
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
set(stb_INCLUDE_DIRS ${stb_SOURCE_DIR})
CPMAddPackage(
  NAME assimp
  GITHUB_REPOSITORY assimp/assimp
  GIT_TAG master
  )
set(assimp_INCLUDE_DIRS ${assimp_SOURCE_DIR}/include)

