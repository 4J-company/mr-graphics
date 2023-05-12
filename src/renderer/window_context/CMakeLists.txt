target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/window_context.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/window_context.hpp
  ${CMAKE_CURRENT_LIST_DIR}/window_system/window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/window_system/window.hpp)
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/window_context.hpp)

