#ifndef _TYPES_H_
#define _TYPES_H_

struct MeshDrawInfo {
  uint mesh_offset; // TODO(dk6): maybe it is unusual field
  uint instance_offset; // TODO(dk6): maybe it is unusual field
  uint material_buffer_id;
  uint transforms_buffer_id;
};

#endif // _TYPES_H_