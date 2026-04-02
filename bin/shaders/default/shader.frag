/**/
#version 450
layout(location = 0) out vec4 OutPos;
layout(location = 1) out vec4 OutNIsShade;
layout(location = 2) out vec4 OutMR;
layout(location = 3) out vec4 OutEmissive;
layout(location = 4) out vec4 OutOcclusion;
layout(location = 5) out vec4 OutColorTrans;

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in flat uint materialid;
layout(location = 4) in flat uint instance_id;

#ifdef ENABLE_CULLING_VISUALIZATION
#include "culling/culling.h"
layout(location = 5) in flat uint visible_at_stash;
#endif // ENABLE_CULLING_VISUALIZATION

#include "pbr_params.h"

void main()
{
  vec4 bckg_color = vec4(0.3, 0.47, 0.8, 1);

  OutPos = vec4(position.xyz, uintBitsToFloat(instance_id));
  // OutPos = vec4(position.xyz, 1.0);

#ifdef ENABLE_CULLING_VISUALIZATION
  float coef = 1;
  float is_shade = 0;
  if (is_shade == 1.0) {
    coef = 0.1;
  }
  OutOcclusion = vec4(0);
  OutEmissive = vec4(0);
  OutMR = vec4(0.1, 0, 0.1, 1);

  if (!IS_INSTANCE_IN_FRUSTUM(visible_at_stash)) {
    OutNIsShade = vec4(normal, is_shade);
    OutColorTrans = vec4(0.1, 0.5, 0, 1) * coef;
    return;
  } else if (IS_INSTANCE_WAS_OCCLUDED(visible_at_stash)) {
    OutNIsShade = vec4(normal, is_shade);
    OutColorTrans = vec4(0, 0, 1, 1) * coef;
    return;
  } else if (!IS_INSTANCE_ON_SCREEN(visible_at_stash)) {
    OutNIsShade = vec4(normal, is_shade);
    OutColorTrans = vec4(1, 0.8, 0, 1) * coef;
    return;
  }
#endif // ENABLE_CULLING_VISUALIZATION

  // OutNIsShade = vec4(normal, 0);
  // OutColorTrans = vec4(0.8, 0.47, 0.30, 1);
  // return;

  OutNIsShade = vec4(normal, 1);

  OutMR         = get_metallic_roughness_color(materialid, texcoord);
  OutEmissive   = get_emissive_color(materialid, texcoord);
  OutOcclusion  = get_occlusion_color(materialid, texcoord);
  OutColorTrans = get_base_color(materialid, texcoord);
}
