#ifndef __MR_MESH_HPP_
#define __MR_MESH_HPP_

#include "pch.hpp"
#include "resources/resources.hpp"

namespace mr {

  class Mesh {
  private:
    std::vector<VertexBuffer> _vbufs;
    IndexBuffer _ibuf;

    /**
 * @brief Atomic counter tracking the number of Mesh instances.
 *
 * This counter is used internally to maintain a thread-safe count
 * of Mesh instances, ensuring proper management during construction,
 * move operations, and destruction.
 */
std::atomic<int> _instance_count = 0;

  public:
    /**
 * @brief Default constructor.
 *
 * Initializes an empty Mesh with no associated vertex or index buffers.
 */
Mesh() = default;

    Mesh(std::vector<VertexBuffer>, IndexBuffer) noexcept;

    /**
     * @brief Move constructs a Mesh by transferring ownership of its resources.
     *
     * Transfers the vertex buffers, index buffer, and instance count from the specified Mesh.
     * The source Mesh is left in a valid but unspecified state. This operation is noexcept.
     *
     * @param other The Mesh instance from which to move resources.
     */
    Mesh(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibuf = std::move(other._ibuf);
      _instance_count = std::move(other._instance_count.load());
    }

    /**
     * @brief Move assignment operator for Mesh.
     *
     * Transfers ownership of the internal resources—including vertex buffers, the index buffer,
     * and the instance count—from the specified Mesh into this Mesh. The source Mesh is left in a
     * valid but unspecified state.
     *
     * @param other The Mesh object to move from.
     * @return Reference to this Mesh instance.
     */
    Mesh &operator=(Mesh &&other) noexcept
    {
      _vbufs = std::move(other._vbufs);
      _ibuf = std::move(other._ibuf);
      _instance_count = std::move(other._instance_count.load());

      return *this;
    }

    /**
 * @brief Returns a reference to the atomic counter tracking the number of Mesh instances.
 *
 * This non-const member function provides thread-safe access to the internal counter, allowing its value
 * to be observed and modified.
 *
 * @return std::atomic<int>& Reference to the atomic instance count.
 */
std::atomic<int> &num_of_instances() noexcept { return _instance_count; }
    /**
 * @brief Retrieves the current number of mesh instances.
 *
 * This function returns the current value of the instance count maintained as an atomic integer,
 * providing thread-safe access to the number of active Mesh instances.
 *
 * @return int The current instance count.
 */
int num_of_instances() const noexcept { return _instance_count.load(); }
    /**
     * @brief Binds the mesh's vertex and index buffers to the provided command unit.
     *
     * Populates temporary arrays with buffer handles derived from the mesh's vertex buffers and binds them,
     * along with the index buffer, to the command unit for rendering.
     */
    void bind(mr::CommandUnit &unit) const noexcept {
      static std::array<vk::Buffer, 16> vbufs {};
      static std::array<vk::DeviceSize, 16> offsets {};

      for (int i = 0; i < _vbufs.size(); i++) {
        vbufs[i] = _vbufs[i].buffer();
      }

      unit->bindVertexBuffers(0,
          std::span{vbufs.data(), _vbufs.size()},
          std::span{offsets.data(), _vbufs.size()});
      unit->bindIndexBuffer(_ibuf.buffer(), 0, _ibuf.index_type());
    }
    /**
     * @brief Issues an indexed draw call for the mesh.
     *
     * Commands the provided command unit to render the mesh using indexed drawing. The draw call
     * uses the element count from the mesh's index buffer and the mesh's instance count, with base
     * index, vertex offset, and instance offset all set to 0.
     */
    void draw(mr::CommandUnit &unit) const noexcept {
      unit->drawIndexed(_ibuf.element_count(), num_of_instances(), 0, 0, 0);
    }
  };
}     // namespace mr

#endif // __MR_MESH_HPP_
