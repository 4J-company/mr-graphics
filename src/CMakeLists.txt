file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR} *.cpp)
list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_sources(${CMAKE_PROJECT_NAME} PUBLIC main.cpp)
target_sources(${CMAKE_PROJECT_NAME}
  PUBLIC
  FILE_SET modules TYPE CXX_MODULES FILES
  ${SOURCES}
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  )
 target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}/pch.hpp
   )

