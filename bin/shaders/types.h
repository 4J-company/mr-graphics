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

struct CameraData {
  mat4 vp;
  mat4 view;
  mat4 proj;
  float near;
  float far;
  vec3 pos;
  vec3 dir;
  float fov;
  float gamma;
  float speed;
  float sens;
  vec4 frustum_planes[6];
};


#endif // _TYPES_H_