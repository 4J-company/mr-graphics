#include "resources/buffer/buffer.hpp"

// constructor
mr::Buffer::Buffer(const VulkanState &state, size_t byte_size,
                   vk::BufferUsageFlags usage_flag,
                   vk::MemoryPropertyFlags memory_properties)
  : _state(&state)
  , _size(byte_size)
  , _usage_flags(usage_flag)
  , _memory_properties(memory_properties)
{
  vk::BufferCreateInfo buffer_create_info {
    .size = _size,
    .usage = _usage_flags,
    .sharingMode = vk::SharingMode::eExclusive,
  };

  _buffer = state.device().createBufferUnique(buffer_create_info).value;

  vk::MemoryRequirements mem_requirements =
    state.device().getBufferMemoryRequirements(_buffer.get());
  vk::MemoryAllocateInfo alloc_info {
    .allocationSize = mem_requirements.size,
    .memoryTypeIndex = find_memory_type(
      state, mem_requirements.memoryTypeBits, _memory_properties),
  };
  _memory = state.device().allocateMemoryUnique(alloc_info).value;
  state.device().bindBufferMemory(_buffer.get(), _memory.get(), 0);
}

// resize
void resize(size_t size) {}

// find memory type
uint32_t mr::Buffer::find_memory_type(
    const VulkanState &state,
    uint32_t filter,
    vk::MemoryPropertyFlags properties) noexcept
{
  vk::PhysicalDeviceMemoryProperties mem_properties =
    state.phys_device().getMemoryProperties();

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags &
                                properties) == properties) {
      return i;
    }
  }

  ASSERT(false, "Cannot find memory format");
  return 0;
}

mr::DeviceBuffer & mr::DeviceBuffer::write(std::span<const std::byte> src)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  auto buf = HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  buf.write(src);

  vk::BufferCopy buffer_copy {.size = src.size()};

  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
  command_unit.end();

  vk::SubmitInfo submit_info = command_unit.submit_info();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

  return *this;
}

mr::HostBuffer & mr::HostBuffer::write(std::span<const std::byte> src)
{
  ASSERT(_state != nullptr);
  ASSERT(src.data());
  ASSERT(src.size() <= _size);

  memcpy(_mapped_data.map(), src.data(), src.size());
  _mapped_data.unmap();
  return *this;
}

std::span<std::byte> mr::HostBuffer::read() noexcept
{
  if (not _mapped_data.mapped()) {
    _mapped_data.map();
  }
  return std::span(reinterpret_cast<std::byte *>(_mapped_data.get()), _size);
}

std::vector<std::byte> mr::HostBuffer::copy() noexcept
{
  std::vector<std::byte> data(_size);
  if (_mapped_data.mapped()) {
    memcpy(data.data(), _mapped_data.get(), _size);
  } else {
    memcpy(data.data(), _mapped_data.map(), _size);
    _mapped_data.unmap();
  }
  return data;
}

mr::HostBuffer::~HostBuffer() {}

// ----------------------------------------------------------------------------
// Data mapper
// ----------------------------------------------------------------------------

mr::HostBuffer::MappedData::MappedData(HostBuffer &buf) : _buf(&buf) {}

mr::HostBuffer::MappedData::~MappedData()
{
  if (_data != nullptr) {
    _buf->_state->device().unmapMemory(_buf->_memory.get());
  }
}

mr::HostBuffer::MappedData & mr::HostBuffer::MappedData::operator=(MappedData &&other) noexcept
{
  std::swap(_buf, other._buf);
  std::swap(_data, other._data);
  return *this;
}

mr::HostBuffer::MappedData::MappedData(MappedData &&other) noexcept
{
  std::swap(_buf, other._buf);
  std::swap(_data, other._data);
}

void * mr::HostBuffer::MappedData::map(size_t offset, size_t size) noexcept
{
  ASSERT(_data == nullptr);
  ASSERT(offset < size);

  _buf->_state->device().mapMemory(_buf->_memory.get(), offset, size, {}, &_data);
  return _data;
}

void * mr::HostBuffer::MappedData::map(size_t offset) noexcept
{
  return map(offset, _buf->_size);
}

void mr::HostBuffer::MappedData::unmap() noexcept
{
  ASSERT(_data != nullptr);
  _buf->_state->device().unmapMemory(_buf->_memory.get());
  _data = nullptr;
}

bool mr::HostBuffer::MappedData::mapped() const noexcept { return _data != nullptr; }
void * mr::HostBuffer::MappedData::get() noexcept { return _data; }

