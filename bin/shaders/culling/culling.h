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
  // Bit 3 - Marks is frustum visibilit was already calculated at first pass
  uint visibility_bits;
  uint mesh_culling_data_index;
};

#define INSTANCE_IN_FRUSTUM_BIT             0
#define INSTANCE_WAS_OCCLUDED_BIT           1
#define INSTANCE_RENDERER_AT_FIRST_PASS_BIT 2
#define INSTANCE_FRUSTUM_CALCULATED_BIT     3
#define INSTANCE_ON_SCREEN_BIT              4 // for debug culling

#define IS_INSTANCE_IN_FRUSTUM(v)                 (((v) & (1 << INSTANCE_IN_FRUSTUM_BIT))             != 0)
#define IS_INSTANCE_WAS_OCCLUDED(v)               (((v) & (1 << INSTANCE_WAS_OCCLUDED_BIT))           != 0)
#define IS_INSTANCE_RENDERER_AT_FIRST_PASS(v)     (((v) & (1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT)) != 0)
#define IS_INSTANCE_FRUSTUM_CALCULATED(v)         (((v) & (1 << INSTANCE_FRUSTUM_CALCULATED_BIT)) != 0)
#define IS_INSTANCE_ON_SCREEN(v)                  (((v) & (1 << INSTANCE_ON_SCREEN_BIT)) != 0)

#define SET_INSTANCE_IN_FRUSTUM(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_IN_FRUSTUM_BIT)) : ((v) & ~((1 << INSTANCE_IN_FRUSTUM_BIT))))
#define SET_INSTANCE_WAS_OCCLUDED(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_WAS_OCCLUDED_BIT)) : ((v) & ~((1 << INSTANCE_WAS_OCCLUDED_BIT))))
#define SET_INSTANCE_RENDERER_AT_FIRST_PASS(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT)) : ((v) & ~((1 << INSTANCE_RENDERER_AT_FIRST_PASS_BIT))))
#define SET_INSTANCE_FRUSTUM_CALCULATED(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_FRUSTUM_CALCULATED_BIT)) : ((v) & ~((1 << INSTANCE_FRUSTUM_CALCULATED_BIT))))
#define SET_INSTANCE_ON_SCREEN(v, value) \
  ((value) ? ((v) | (1 << INSTANCE_ON_SCREEN_BIT)) : ((v) & ~((1 << INSTANCE_ON_SCREEN_BIT))))

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
