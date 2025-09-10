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
mr::uint
mr::Buffer::find_memory_type(const VulkanState &state, uint filter,
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

  assert(false); // cant find format
  return 0;
}

void mr::HostBuffer::_write(std::span<const std::byte> data) noexcept
{
  ASSERT(data.size() <= _size);
  ASSERT(_state != nullptr);

  memcpy(_mapped_data.map(), data.data(), data.size());
  _mapped_data.unmap();
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

void mr::DeviceBuffer::_write(std::span<const std::byte> data) noexcept
{
  assert(_state != nullptr);
  auto buf =
    HostBuffer(*_state, _size, vk::BufferUsageFlagBits::eTransferSrc);
  buf.write(data);

  vk::BufferCopy buffer_copy {.size = data.size()};

  static CommandUnit command_unit(*_state);
  command_unit.begin();
  command_unit->copyBuffer(buf.buffer(), _buffer.get(), {buffer_copy});
  vk::SubmitInfo submit_info = command_unit.end();

  auto fence = _state->device().createFenceUnique({}).value;
  _state->queue().submit(submit_info, fence.get());
  _state->device().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);
}

mr::HostBuffer::HostBuffer(
  const VulkanState &state, std::size_t size,
  vk::BufferUsageFlags usage_flags,
  vk::MemoryPropertyFlags memory_properties)
    : Buffer(state, size, usage_flags,
             memory_properties |
               vk::MemoryPropertyFlagBits::eHostVisible |
               vk::MemoryPropertyFlagBits::eHostCoherent)
    , _mapped_data(*this)
{
}


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
  if (size == max_size) {
    size = _buf->_size;
  }
  ASSERT(offset < size);
  _buf->_state->device().mapMemory(_buf->_memory.get(), offset, size, {}, &_data);
  return _data;
}

void mr::HostBuffer::MappedData::unmap() noexcept
{
  ASSERT(_data != nullptr);
  _buf->_state->device().unmapMemory(_buf->_memory.get());
  _data = nullptr;
}

bool mr::HostBuffer::MappedData::mapped() const noexcept { return _data != nullptr; }
void * mr::HostBuffer::MappedData::get() noexcept { return _data; }