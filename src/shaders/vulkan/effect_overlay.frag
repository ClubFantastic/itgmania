#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(set = 1, binding = 0) uniform sampler2D u_Texture0;
layout(constant_id = 2) const bool TEXTURE_ENABLED = true;
layout(location = 0) out vec4 FragColor;
void main() {
    vec4 base = v_Color;
    vec4 blend = TEXTURE_ENABLED ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    vec3 result;
    result.r = (base.r < 0.5) ? 2.0*base.r*blend.r : 1.0 - 2.0*(1.0-base.r)*(1.0-blend.r);
    result.g = (base.g < 0.5) ? 2.0*base.g*blend.g : 1.0 - 2.0*(1.0-base.g)*(1.0-blend.g);
    result.b = (base.b < 0.5) ? 2.0*base.b*blend.b : 1.0 - 2.0*(1.0-base.b)*(1.0-blend.b);
    FragColor = vec4(result, base.a * blend.a);
}
