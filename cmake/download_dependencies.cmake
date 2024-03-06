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
  NAME assimp
  GITHUB_REPOSITORY assimp/assimp
  GIT_TAG master
  )
set(assimp_INCLUDE_DIRS ${assimp_SOURCE_DIR}/include)

