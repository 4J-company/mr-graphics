#ifndef _TYPES_H_
#define _TYPES_H_

struct MeshDrawInfo {
  uint mesh_offset;
  uint instance_offset;
  uint material_buffer_id;
  uint instance_render_info_buffer_id;
};

struct InstanceDrawInfo {
  uint transforms_index;
};

#endif // _TYPES_H_