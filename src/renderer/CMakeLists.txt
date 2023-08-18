# window system files
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/window_context)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_context.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_context.hpp
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_system/window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_system/window.hpp)
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/window_context/window_context.hpp)

# render resources files
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/attachment/attachment.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/attachment/attachment.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/buffer/buffer.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/buffer/buffer.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/command_unit/command_unit.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/command_unit/command_unit.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/descriptor/descriptor.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/descriptor/descriptor.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/framebuffer/framebuffer.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/framebuffer/framebuffer.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/images/image.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/images/image.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/pipeline.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/pipeline.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/graphics_pipeline.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/graphics_pipeline.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/compute_pipeline.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/pipelines/compute_pipeline.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/shaders/shader.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/shaders/shader.cpp

  ${CMAKE_CURRENT_LIST_DIR}/resources/texture/sampler/sampler.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/texture/sampler/sampler.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/texture/texture.hpp
  ${CMAKE_CURRENT_LIST_DIR}/resources/texture/texture.cpp

  ${CMAKE_CURRENT_LIST_DIR}/timer/timer.hpp
  ${CMAKE_CURRENT_LIST_DIR}/timer/timer.cpp
 )
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp)
