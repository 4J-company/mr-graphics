#ifndef __MR_FRAMEDATA_HPP_
#define __MR_FRAMEDATA_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"

namespace mr {
  struct Viewport {
      vk::Viewport viewport {};
      vk::Rect2D scissors {};
  };

  class RenderContext;

  class FrameData {
      friend class RenderContext;

    private:
      static inline constexpr size_t max_gbuffers = 6;

      Extent _extent;
      Viewport _viewport;
      const SwapchainImage &_target;
      beman::inplace_vector<ColorAttachmentImage, max_gbuffers> _gbuffers;
      DepthImage _depthbuffer;

    public:
      FrameData(const VulkanState &state, Extent extent, const SwapchainImage &target);

      void resize(size_t width, size_t height);

      vk::Viewport viewport() const { return _viewport.viewport; }

      vk::Rect2D scissors() const { return _viewport.scissors; }
  };
} // namespace mr

#endif // __MR_FRAMEDATA_HPP_
