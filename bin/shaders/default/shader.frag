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

#ifdef ENABLE_CULLING_VISUALIZATION
  if (!IS_INSTANCE_IN_FRUSTUM(visible_at_stash)) {
    OutNIsShade = vec4(vec3(0), 0);
    OutColorTrans = vec4(0, 0, 1, 1);
    return;
  } else if (IS_INSTANCE_WAS_OCCLUDED(visible_at_stash)) {
    OutNIsShade = vec4(vec3(0), 0);
    OutColorTrans = vec4(1, 0, 0, 1);
    return;
  }
#endif // ENABLE_CULLING_VISUALIZATION

  OutPos = vec4(position.xyz, float(instance_id));
  OutNIsShade = vec4(normal, 1);

  OutMR         = get_metallic_roughness_color(materialid, texcoord);
  OutEmissive   = get_emissive_color(materialid, texcoord);
  OutOcclusion  = get_occlusion_color(materialid, texcoord);
  OutColorTrans = get_base_color(materialid, texcoord);
}
