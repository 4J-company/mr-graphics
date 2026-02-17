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
  // Bit 0 - Marks is instance was inside frustum at first culling
  // Bit 1 - Marks is instance was occluded at previous frame
  // Bit 2 - Marks is instance was rendered in first pass
  uint visibility_bits;
  uint mesh_culling_data_index;
};

#define INSTANCE_IN_FRUSTUM_BIT             0
#define INSTANCE_WAS_OCCLUDED_BIT           1
#define INSTANCE_RENDERER_AT_FIRST_PASS_BIT 2

#define IS_INSTANCE_IN_FRUSTUM(v)                 (((v) & (1 << INSTANCE_IN_FRUSTUM_BIT))             != 0)
#define IS_INSTANCE_WAS_OCCLUDED(v)               (((v) & (1 << INSTANCE_WAS_OCCLUDED_BIT))           != 0)
#define IS_INSTANCE_RENDERER_AT_FIRST_PASS_BIT(v) (((v) & (1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT)) != 0)

#define SET_INSTANCE_IN_FRUSTUM(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_IN_FRUSTUM_BIT)) : ((v) & ~((1 << INSTANCE_IN_FRUSTUM_BIT))))
#define SET_INSTANCE_WAS_OCCLUDED(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_WAS_OCCLUDED_BIT)) : ((v) & ~((1 << INSTANCE_WAS_OCCLUDED_BIT))))
#define SET_INSTANCE_RENDERER_AT_FIRST_PASS_BIT(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT)) : ((v) & ~((1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT))))

struct MeshCullingData {
  IndirectCommand command;
  MeshDrawInfo mesh_draw_info;

  uint instance_counter_index;
  uint bound_box_index;
};

#ifdef COLLECT_CULLING_STAT
struct CullingStats {
  uint total_objects_cnt;
  uint outside_frustum_objects_number;
  uint occluded_objects_cnt;
};
#endif // COLLECT_CULLING_STAT


#endif // _CULLING_H__