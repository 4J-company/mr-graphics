#include "resources/images/image.hpp"

ter::Image::Image(std::string_view filename) {}

ter::Image::Image(vk::Image image) {}

void ter::Image::switch_layout(vk::ImageLayout layout) {}

void ter::Image::copy_to_host() const {}

void ter::Image::get_pixel(const vk::Extent2D &coords) const {}
