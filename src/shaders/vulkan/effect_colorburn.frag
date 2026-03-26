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
    result.r = (blend.r == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.r) / blend.r);
    result.g = (blend.g == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.g) / blend.g);
    result.b = (blend.b == 0.0) ? 0.0 : max(0.0, 1.0 - (1.0 - base.b) / blend.b);
    FragColor = vec4(result, base.a * blend.a);
}
