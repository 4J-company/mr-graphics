#ifndef _CULLING_H_
#define _CULLING_H_

#include "types.h"
#include "bounds.h"

struct IndirectCommand {
  uint index_count;
  uint instance_count;
  uint first_index;
  int  vertex_offset;
  uint first_instance;
};

struct MeshInstanceCullingData {
  uint transform_index;
  uint visible_last_frame;
  uint mesh_culling_data_index;
};

struct MeshCullingData {
  IndirectCommand command;
  MeshDrawInfo mesh_draw_info;

  uint instance_counter_index;
  uint bound_box_index;
};

#endif // _CULLING_H__