#ifndef __DUMMY_PRESENTER_HPP_
#define __DUMMY_PRESENTER_HPP_

#include "pch.hpp"
#include "resources/images/image.hpp"
#include "presenter.hpp"

namespace mr {
inline namespace graphics {
  class DummyPresenter : public Presenter, public ResourceBase<DummyPresenter> {
  public:
    constexpr static uint32_t images_number = 3;
  private:
    beman::inplace_vector<ColorAttachmentImage, images_number> _images;

    uint32_t _image_index = 0;

  public:
    DummyPresenter(const RenderContext &parent, Extent extent = {800, 600});

    DummyPresenter(DummyPresenter &&other) noexcept = default;
    DummyPresenter & operator=(DummyPresenter &&other) noexcept = default;
    ~DummyPresenter() = default;

    // Return rendering attachment info with target image
    vk::RenderingAttachmentInfoKHR target_image_info() noexcept final;
    void present() noexcept final;
  };

  MR_DECLARE_HANDLE(DummyPresenter);
}
} // namespace mr
#endif // __DUMMY_PRESENTER_HPP_
