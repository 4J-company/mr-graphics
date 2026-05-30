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

// TODO(dk6): use this and store bounds together
struct Bounds {
  BoundSphere sphere;
  BoundBox box;
};

// Get bound box in world-space
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

// Get bound box in world-space
BoundSphere transform_bound_sphere(BoundSphere bs, mat4 transform)
{
  BoundSphere res;
  res.center = (transform * vec4(bs.center, 1.0)).xyz;
  float scale_x = length(transform[0].xyz);
  float scale_y = length(transform[1].xyz);
  float scale_z = length(transform[2].xyz);
  float scale_max = max(scale_x, max(scale_y, scale_z));
  res.radius = bs.radius * scale_max;
  return res;
}

// Closest point on AABB surface to an arbitrary world-space point.
// If the query point is outside the box, returns clamp(point, min, max).
// Returns true if point is inside the box.
bool closest_point_on_bound_box(vec3 point, BoundBox bb, out vec3 closest_point)
{
  vec3 bmin = bb.min.xyz;
  vec3 bmax = bb.max.xyz;

  bool inside = point.x >= bmin.x && point.x <= bmax.x &&
                point.y >= bmin.y && point.y <= bmax.y &&
                point.z >= bmin.z && point.z <= bmax.z;
  if (inside) {
    return true;
  }
  closest_point = clamp(point, bmin, bmax);
  return false;
}

// TODO(dk6): Search in internet more optimize way to do it
vec4 get_bound_box_screen_rectangle(BoundBox bb, mat4 proj)
{
  vec4 corners[8];
  corners[0] = proj * vec4(bb.min.x, bb.min.y, bb.min.z, 1);
  corners[1] = proj * vec4(bb.max.x, bb.min.y, bb.min.z, 1);
  corners[2] = proj * vec4(bb.max.x, bb.max.y, bb.min.z, 1);
  corners[3] = proj * vec4(bb.min.x, bb.max.y, bb.min.z, 1);
  corners[4] = proj * vec4(bb.min.x, bb.min.y, bb.max.z, 1);
  corners[5] = proj * vec4(bb.max.x, bb.min.y, bb.max.z, 1);
  corners[6] = proj * vec4(bb.max.x, bb.max.y, bb.max.z, 1);
  corners[7] = proj * vec4(bb.min.x, bb.max.y, bb.max.z, 1);
  for (int i = 0; i < 8; i++) {
    corners[i] /= corners[i].w;
  }

  vec4 res = vec4(corners[0].xy, corners[0].xy);
  for (int i = 1; i < 8; i++) {
    res.x = min(res.x, corners[i].x);
    res.y = min(res.y, corners[i].y);
    res.z = max(res.z, corners[i].x);
    res.w = max(res.w, corners[i].y);
  }

  // Flip over Ox
  float tmp = -res.y;
  res.y = -res.w;
  res.w = tmp;

  // rectangle now in [-1; 1] screen coords, convert to [0; 1] texture coords
  res = (res + vec4(1)) / 2;

  return res;
}

bool is_bound_sphere_dont_clips_camera(vec3 c, float r, float znear) {
  float depth = -c.z; // inverse view space - in opengl we watch at negative Oz
  // Check near plane clips with sphere
  if (depth < r + znear) {
    return false;
  }

  float czr2 = depth * depth - r * r;
  if (czr2 <= 0.0) {
    return false;
  }

  return true;
}

bool get_bound_sphere_screen_rectangle(vec3 c, float r, float znear, float P00, float P11, out vec4 aabb)
{
  float depth = -c.z; // inverse view space - in opengl we watch at negative Oz
  // Check near plane clips with sphere
  if (depth < r + znear) {
    return false;
  }

  float czr2 = depth * depth - r * r;
  if (czr2 <= 0.0) {
    return false;  // Is this important?
  }

  float vx = sqrt(c.x * c.x + czr2);
  float vy = sqrt(c.y * c.y + czr2);

  // Calculate bounds
  float minx = (vx * c.x - depth * r) / (vx * depth + c.x * r);
  float maxx = (vx * c.x + depth * r) / (vx * depth - c.x * r);

  float miny = (vy * c.y - depth * r) / (vy * depth + c.y * r);
  float maxy = (vy * c.y + depth * r) / (vy * depth - c.y * r);

  // Apply projection
  aabb = vec4(minx * P00, miny * P11, maxx * P00, maxy * P11);

  // Clip space [-1,1] → UV space [0,1] (with reversing Y and flipping over Ox)
  aabb = aabb.xwzy * vec4(0.5, -0.5, 0.5, -0.5) + vec4(0.5);

  return true;
}

// Get max of rectangle width and height
float bounds_rectangle_max_dim(vec4 rectangle)
{
  return max(abs(rectangle.x - rectangle.z), abs(rectangle.y - rectangle.w));
}

// Choose bounds with minimum screen rectangle dimension (max of W/H) - it will be best for HiZ sampling
// params: znear - projection distance,
//         rectangle - screen rectangle (pos0.x, pos0.y, pos1.x, pos1.y)
// returns: false if bound sphere is behind us - it is invisible
bool get_best_screen_rectangle(BoundBox bb, BoundSphere bs, float znear, mat4 proj, mat4 vp,
                               out vec4 rectangle, out bool is_bs)
{
  float p00 = proj[0][0], p11 = proj[1][1];
  vec4 bs_rect;
  if (!get_bound_sphere_screen_rectangle(bs.center, bs.radius, znear, p00, p11, bs_rect)) {
    is_bs = true;
    return false;
  }

  vec4 bb_rect = get_bound_box_screen_rectangle(bb, vp);

  float bb_rect_size = bounds_rectangle_max_dim(bb_rect);
  float bs_rect_size = bounds_rectangle_max_dim(bs_rect);

  if (bb_rect_size < bs_rect_size) {
    rectangle = bb_rect;
    is_bs = false;
  } else {
    rectangle = bs_rect;
    is_bs = true;
  }

  return true;
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

bool is_bound_box_frustum_visible(BoundBox bb, in vec4 frustum_planes[6])
{
  for (int i = 0; i < 6; i++) {
    if (is_bound_box_not_visible(frustum_planes[i], bb)) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------
// Bound sphere base functionality
// -----------------------------------------

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
