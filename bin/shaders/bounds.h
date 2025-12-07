#ifndef _BOUNDS_H_
#define _BOUNDS_H_

struct BoundBox {
  vec4 min;
  vec4 max;
};

struct BoundSphere {
  vec3 center;
  float radius;
};

BoundBox transform_bound_box(BoundBox bb, mat4 transform)
{
  vec3 corners[8];
  corners[0] = (transform * vec4(bb.min.x, bb.min.y, bb.min.z, 1)).xyz;
  corners[1] = (transform * vec4(bb.max.x, bb.min.y, bb.min.z, 1)).xyz;
  corners[2] = (transform * vec4(bb.max.x, bb.max.y, bb.min.z, 1)).xyz;
  corners[3] = (transform * vec4(bb.min.x, bb.max.y, bb.min.z, 1)).xyz;
  corners[4] = (transform * vec4(bb.min.x, bb.min.y, bb.max.z, 1)).xyz;
  corners[5] = (transform * vec4(bb.max.x, bb.min.y, bb.max.z, 1)).xyz;
  corners[6] = (transform * vec4(bb.max.x, bb.max.y, bb.max.z, 1)).xyz;
  corners[7] = (transform * vec4(bb.min.x, bb.max.y, bb.max.z, 1)).xyz;

  BoundBox res;
  res.min = vec4(corners[0], 0);
  res.max = vec4(corners[0], 0);
  for (uint i = 1; i < 8; i++) {
    res.min = vec4(min(res.min.xyz, corners[i]), 0);
    res.max = vec4(max(res.max.xyz, corners[i]), 0);
  }
  return res;
}

vec4 get_bound_box_screen_rectangle(BoundBox bb, mat4 mvp)
{
  vec4 corners[8];
  corners[0] = mvp * vec4(bb.min.x, bb.min.y, bb.min.z, 1);
  corners[1] = mvp * vec4(bb.max.x, bb.min.y, bb.min.z, 1);
  corners[2] = mvp * vec4(bb.max.x, bb.max.y, bb.min.z, 1);
  corners[3] = mvp * vec4(bb.min.x, bb.max.y, bb.min.z, 1);
  corners[4] = mvp * vec4(bb.min.x, bb.min.y, bb.max.z, 1);
  corners[5] = mvp * vec4(bb.max.x, bb.min.y, bb.max.z, 1);
  corners[6] = mvp * vec4(bb.max.x, bb.max.y, bb.max.z, 1);
  corners[7] = mvp * vec4(bb.min.x, bb.max.y, bb.max.z, 1);
  for (int i = 0; i < 8; i++) {
    corners[i] /= corners[i].w;
  }

  vec4 result = vec4(corners[0].x, corners[0].y, corners[0].x, corners[0].y);
  for (int i = 1; i < 8; i++) {
    result.x = min(result.x, corners[i].x);
    result.y = min(result.y, corners[i].y);
    result.z = max(result.z, corners[i].x);
    result.w = max(result.w, corners[i].y);
  }
  return result;
}

bool is_bound_box_not_visible(vec4 plane, BoundBox bb)
{
  vec3 positive = bb.min.xyz;
  vec3 negative = bb.max.xyz;

  if (plane.x >= 0) {
    positive.x = bb.max.x;
    negative.x = bb.min.x;
  }
  if (plane.y >= 0) {
    positive.y = bb.max.y;
    negative.y = bb.min.y;
  }
  if (plane.z >= 0) {
    positive.z = bb.max.z;
    negative.z = bb.min.z;
  }

  if (dot(plane.xyz, negative) + plane.w > 0) {
    // If the nearest point is outside the plane,
    //   the box is completely outside or intersects
    return false;
  }

  if (dot(plane.xyz, positive) + plane.w < 0) {
    // If the far point is beyond the plane, the box is completely outside
    return true;
  }

  return false;
}

BoundSphere bb2bs(BoundBox bb)
{
  BoundSphere bs;
  bs.center = (bb.max.xyz + bb.min.xyz) / 2;
  bs.radius = length(bb.max.xyz - bb.min.xyz) / 2;
  return bs;
}

bool is_bound_sphere_not_visible(vec4 plane, BoundSphere bs) {
  return !(dot(plane, vec4(bs.center, 1)) > -bs.radius);
}

#endif // _BOUNDS_H_