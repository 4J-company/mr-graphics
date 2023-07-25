# project files
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/window_context)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp
  )
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp)
